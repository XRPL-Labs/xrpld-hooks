//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2014 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/app/tx/impl/SetHook.h>

#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/Log.h>
#include <ripple/ledger/ApplyView.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STTx.h>
#include <algorithm>
#include <cstdint>
#include <stdio.h>
#include <vector>
#include <stack>
#include <string>
#include <utility>
#include <ripple/app/tx/applyHook.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <functional>
#include <wasmedge/wasmedge.h>
#include <exception>
#include <tuple>

#define DEBUG_GUARD_CHECK 0
#define HS_ACC() ctx.tx.getAccountID(sfAccount) << "-" << ctx.tx.getTransactionID()

namespace ripple {

// RH TODO test overflow on leb128 detection
// web assembly contains a lot of run length encoding in LEB128 format
inline uint64_t
parseLeb128(std::vector<unsigned char>& buf, int start_offset, int* end_offset)
{
    uint64_t val = 0, shift = 0, i = start_offset;
    while (i < buf.size())
    {
        int b = (int)(buf[i]);
        uint64_t last = val;
        val += (b & 0x7F) << shift;
        if (val < last)
        {
            // overflow
            throw std::overflow_error { "leb128 overflow" };
        }
        ++i;
        if (b & 0x80)
        {
            shift += 7;
            continue;
        }
        *end_offset = i;
        return val;
    }
    return 0;
}

// this macro will return temMALFORMED if i ever exceeds the end of the hook
#define CHECK_SHORT_HOOK()\
{\
    if (i >= hook.size())\
    {\
        JLOG(ctx.j.trace())\
            << "HookSet(" << hook::log::SHORT_HOOK << ")[" << HS_ACC() << "]: "\
            << "Malformed transaction: Hook truncated or otherwise invalid\n";\
        return {false, 0};\
    }\
}

// checks the WASM binary for the appropriate required _g guard calls and rejects it if they are not found
// start_offset is where the codesection or expr under analysis begins and end_offset is where it ends
// returns {valid, worst case instruction count}
// may throw overflow_error
std::pair<bool, uint64_t>
check_guard(
        SetHookCtx& ctx,
        ripple::Blob& hook, int codesec,
        int start_offset, int end_offset, int guard_func_idx, int last_import_idx)
{

    if (end_offset <= 0) end_offset = hook.size();
    int block_depth = 0;
    int mode = 1; // controls the state machine for searching for guards
                  // 0 = looking for guard from a trigger point (loop or function start)
                  // 1 = looking for a new trigger point (loop);
                  // currently always starts at 1 no-top-of-func check, see above block comment

    std::stack<uint64_t> stack; // we track the stack in mode 0 to work out if constants end up in the guard function
    std::map<uint32_t, uint64_t> local_map; // map of local variables since the trigger point
    std::map<uint32_t, uint64_t> global_map; // map of global variables since the trigger point

    // block depth level -> { largest guard, rolling instruction count } 
    std::map<int, std::pair<uint32_t, uint64_t>> instruction_count;

                        // largest guard  // instr ccount
    instruction_count[0] = {1,                  0};

    if (DEBUG_GUARD_CHECK)
        printf("\n\n\nstart of guard analysis for codesec %d\n", codesec);

    for (int i = start_offset; i < end_offset; )
    {

        if (DEBUG_GUARD_CHECK)
        {
            printf("->");
            for (int z = i; z < 16 + i && z < end_offset; ++z)
                printf("%02X", hook[z]);
            printf("\n");
        }

        int instr = hook[i++]; CHECK_SHORT_HOOK();
        instruction_count[block_depth].second++;

        if (instr == 0x10) // call instr
        {
            int callee_idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (DEBUG_GUARD_CHECK)
                printf("%d - call instruction at %d -- call funcid: %d\n", mode, i, callee_idx);

            // disallow calling of user defined functions inside a hook
            if (callee_idx > last_import_idx)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::CALL_ILLEGAL << ")[" << HS_ACC() << "]: GuardCheck "
                    << "Hook calls a function outside of the whitelisted imports "
                    << "codesec: " << codesec << " hook byte offset: " << i;
                return {false, 0};
            }

            if (callee_idx == guard_func_idx)
            {
                // found!
                if (mode == 0)
                {

                    if (stack.size() < 2)
                    {
                        JLOG(ctx.j.trace())
                            << "HookSet(" << hook::log::GUARD_PARAMETERS << ")[" << HS_ACC() << "]: GuardCheck "
                            << "_g() called but could not detect constant parameters "
                            << "codesec: " << codesec << " hook byte offset: " << i << "\n";
                        return {false, 0};
                    }

                    uint64_t a = stack.top();
                    stack.pop();
                    uint64_t b = stack.top();
                    stack.pop();
                    if (DEBUG_GUARD_CHECK)
                        printf("FOUND: GUARD(%llu, %llu), codesec: %d offset %d\n", a, b, codesec, i);

                    if (b <= 0)
                    {
                        // 0 has a special meaning, generally it's not a constant value
                        // < 0 is a constant but negative, either way this is a reject condition
                        JLOG(ctx.j.trace()) << "HookSet(" << hook::log::GUARD_PARAMETERS << ")"
                            << "[" << HS_ACC() << "]: GuardCheck "
                            << "_g() called but could not detect constant parameters "
                            << "codesec: " << codesec << " hook byte offset: " << i << "\n";
                        return {false, 0};
                    }

                    // update the instruction count for this block depth to the largest possible guard
                    if (instruction_count[block_depth].first < a)
                    {
                        instruction_count[block_depth].first = a;
                        if (DEBUG_GUARD_CHECK)
                        {
                            JLOG(ctx.j.trace())
                                << "HookDebug[" << HS_ACC() << "]: GuardCheck "
                                << "Depth " << block_depth << " guard: " << a;
                        }
                    }

                    // clear stack and maps
                    while (stack.size() > 0)
                        stack.pop();
                    local_map.clear();
                    global_map.clear();
                    mode = 1;
                }
            }
            continue;
        }

        if (instr == 0x11) // call indirect [ we don't allow guard to be called this way ]
        {
            JLOG(ctx.j.trace()) << "HookSet(" << hook::log::CALL_INDIRECT << ")[" << HS_ACC() << "]: GuardCheck "
                << "Call indirect detected and is disallowed in hooks "
                << "codesec: " << codesec << " hook byte offset: " << i;
            return {false, 0};
            /*
            if (DEBUG_GUARD_CHECK)
                printf("%d - call_indirect instruction at %d\n", mode, i);
            parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            ++i; CHECK_SHORT_HOOK(); //absorb 0x00 trailing
            continue;
            */
        }

        // unreachable and nop instructions
        if (instr == 0x00 || instr == 0x01)
        {
            if (DEBUG_GUARD_CHECK)
                printf("%d - unreachable/nop instruction at %d\n", mode, i);
            continue;
        }

        // branch loop block instructions
        if ((instr >= 0x02 && instr <= 0x0F) || instr == 0x11)
        {
            if (mode == 0)
            {
                JLOG(ctx.j.trace()) << "HookSet(" << hook::log::GUARD_MISSING << ")"
                    << "[" << HS_ACC() << "]: GuardCheck "
                    << "_g() did not occur at start of function or loop statement "
                    << "codesec: " << codesec << " hook byte offset: " << i;
                return {false, 0};
            }

            // execution to here means we are in 'search mode' for loop instructions

            // block instruction
            if (instr == 0x02)
            {
                if (DEBUG_GUARD_CHECK)
                    printf("%d - block instruction at %d\n", mode, i);

                ++i; CHECK_SHORT_HOOK();
                block_depth++;
                instruction_count[block_depth] = {1, 0};
                continue;
            }

            // loop instruction
            if (instr == 0x03)
            {
                if (DEBUG_GUARD_CHECK)
                    printf("%d - loop instruction at %d\n", mode, i);

                ++i; CHECK_SHORT_HOOK();
                mode = 0; // we now search for a guard()
                block_depth++;
                instruction_count[block_depth] = {1, 0};
                continue;
            }

            // if instr
            if (instr == 0x04)
            {
                if (DEBUG_GUARD_CHECK)
                    printf("%d - if instruction at %d\n", mode, i);
                ++i; CHECK_SHORT_HOOK();
                block_depth++;
                instruction_count[block_depth] = {1, 0};
                continue;
            }

            // else instr
            if (instr == 0x05)
            {
                if (DEBUG_GUARD_CHECK)
                    printf("%d - else instruction at %d\n", mode, i);
                continue;
            }

            // branch instruction
            if (instr == 0x0C || instr == 0x0D)
            {
                if (DEBUG_GUARD_CHECK)
                    printf("%d - br instruction at %d\n", mode, i);
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                continue;
            }

            // branch table instr
            if (instr == 0x0E)
            {
                if (DEBUG_GUARD_CHECK)
                    printf("%d - br_table instruction at %d\n", mode, i);
                int vec_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                for (int v = 0; v < vec_count; ++v)
                {
                    parseLeb128(hook, i, &i);
                    CHECK_SHORT_HOOK();
                }
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                continue;
            }
        }


        // parametric instructions | no operands
        if (instr == 0x1A || instr == 0x1B)
        {
            if (DEBUG_GUARD_CHECK)
                printf("%d - parametric  instruction at %d\n", mode, i);
            continue;
        }

        // variable instructions
        if (instr >= 0x20 && instr <= 0x24)
        {
            if (DEBUG_GUARD_CHECK)
                printf("%d - variable local/global instruction at %d\n", mode, i);

            int idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();

            if (mode == 1)
                continue;

            // we need to do stack and map manipualtion to track any possible constants before guard call
            if (instr == 0x20 || instr == 0x23) // local.get idx or global.get idx
            {
                auto& map = ( instr == 0x20 ? local_map : global_map );
                if (map.find(idx) == map.end())
                    stack.push(0); // we always put a 0 in place of a local or global we don't know the value of
                else
                    stack.push(map[idx]);
                continue;
            }

            if (instr == 0x21 || instr == 0x22 || instr == 0x24) // local.set idx or global.set idx
            {
                auto& map = ( instr == 0x21 || instr == 0x22 ? local_map : global_map );

                uint64_t to_store = (stack.size() == 0 ? 0 : stack.top());
                map[idx] = to_store;
                if (instr != 0x22)
                    stack.pop();

                continue;
            }
        }

        // RH TODO support guard consts being passed through memory functions (maybe)

        //memory instructions
        if (instr >= 0x28 && instr <= 0x3E)
        {
            if (DEBUG_GUARD_CHECK)
                printf("%d - variable memory instruction at %d\n", mode, i);

            parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            continue;
        }

        // more memory instructions
        if (instr == 0x3F || instr == 0x40)
        {
            if (DEBUG_GUARD_CHECK)
                printf("%d - memory instruction at %d\n", mode, i);

            ++i; CHECK_SHORT_HOOK();
            if (instr == 0x40) // disallow memory.grow
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::MEMORY_GROW << ")[" << HS_ACC() << "]: GuardCheck "
                    << "Memory.grow instruction not allowed at "
                    << "codesec: " << codesec << " hook byte offset: " << i << "\n";
                return {false, 0};
            }
            continue;
        }

        // int const instrs
        // numeric instructions with immediates
        if (instr == 0x41 || instr == 0x42)
        {

            if (DEBUG_GUARD_CHECK)
                printf("%d - const instruction at %d\n", mode, i);

            uint64_t immediate = parseLeb128(hook, i, &i);
            CHECK_SHORT_HOOK(); // RH TODO enforce i32 i64 size limit

            // in mode 0 we should be stacking our constants and tracking their movement in
            // and out of locals and globals
            stack.push(immediate);
            continue;
        }

        // const instr
        // more numerics with immediates
        if (instr == 0x43 || instr == 0x44)
        {

            if (DEBUG_GUARD_CHECK)
                printf("%d - const float instruction at %d\n", mode, i);

            i += ( instr == 0x43 ? 4 : 8 );
            CHECK_SHORT_HOOK();
            continue;
        }

        // numerics no immediates
        if (instr >= 0x45 && instr <= 0xC4)
        {
            if (DEBUG_GUARD_CHECK)
                printf("%d - numeric instruction at %d\n", mode, i);
            continue;
        }

        // truncation instructions
        if (instr == 0xFC)
        {
            if (DEBUG_GUARD_CHECK)
                printf("%d - truncation instruction at %d\n", mode, i);
            i++; CHECK_SHORT_HOOK();
            parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            continue;
        }

        if (instr == 0x0B)
        {
            if (DEBUG_GUARD_CHECK)
                printf("%d - block end instruction at %d\n", mode, i);

            // end of expression
            if (block_depth == 0)
            break;

            block_depth--;

            if (block_depth < 0)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::BLOCK_ILLEGAL << ")[" << HS_ACC() << "]: GuardCheck "
                    << "Unexpected 0x0B instruction, malformed"
                    << "codesec: " << codesec << " hook byte offset: " << i;
                return {false, 0};
            }

            // perform the instruction count * guard accounting
            instruction_count[block_depth].second +=
                instruction_count[block_depth+1].second * instruction_count[block_depth+1].first;
            instruction_count.erase(block_depth+1);
        }
    }

    JLOG(ctx.j.trace())
        << "HookSet(" << hook::log::INSTRUCTION_COUNT << ")[" << HS_ACC() << "]: GuardCheck "
        << "Total worse-case execution count: " << instruction_count[0].second;

    // RH TODO: don't hardcode this
    if (instruction_count[0].second > 0xFFFFF)
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::INSTRUCTION_EXCESS << ")[" << HS_ACC() << "]: GuardCheck "
            << "Maximum possible instructions exceed 1048575, please make your hook smaller "
            << "or check your guards!";
        return {false, 0};
    }

    // if we reach the end of the code looking for another trigger the guards are installed correctly
    if (mode == 1)
        return {true, instruction_count[0].second};

    JLOG(ctx.j.trace())
        << "HookSet(" << hook::log::GUARD_MISSING << ")[" << HS_ACC() << "]: GuardCheck "
        << "Guard did not occur before end of loop / function. "
        << "Codesec: " << codesec;
    return {false, 0};

}

bool
validateHookGrants(SetHookCtx& ctx, STArray const& hookGrants)
{

    if (hookGrants.size() < 1)
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::GRANTS_EMPTY << ")[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHookGrants empty.";
        return false;
    }

    if (hookGrants.size() > 8)
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::GRANTS_EXCESS << ")[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHookGrants contains more than 8 entries.";
        return false;
    }

    for (auto const& hookGrant : hookGrants)
    {
        auto const& hookGrantObj = dynamic_cast<STObject const*>(&hookGrant);
        if (!hookGrantObj || (hookGrantObj->getFName() != sfHookGrant))
        {
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::GRANTS_ILLEGAL << ")[" << HS_ACC()
                << "]: Malformed transaction: SetHook sfHookGrants did not contain sfHookGrant object.";
            return false;
        }
        else if (!hookGrantObj->isFieldPresent(sfAuthorize) && !hookGrantObj->isFieldPresent(sfHookHash))
        {
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::GRANTS_FIELD << ")[" << HS_ACC()
                << "]: Malformed transaction: SetHook sfHookGrant object did not contain either sfAuthorize "
                << "or sfHookHash.";
            return false;
        }
    }

    return true;
}

bool
validateHookParams(SetHookCtx& ctx, STArray const& hookParams)
{
    for (auto const& hookParam : hookParams)
    {
        auto const& hookParamObj = dynamic_cast<STObject const*>(&hookParam);

        if (!hookParamObj || (hookParamObj->getFName() != sfHookParameter))
        {
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::PARAMETERS_ILLEGAL << ")[" << HS_ACC()
                << "]: Malformed transaction: "
                << "SetHook sfHookParameters contains obj other than sfHookParameter.";
            return false;
        }

        bool nameFound = false;
        for (auto const& paramElement : *hookParamObj)
        {
            auto const& name = paramElement.getFName();

            if (name != sfHookParameterName && name != sfHookParameterValue)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::PARAMETERS_FIELD << ")[" << HS_ACC()
                    << "]: Malformed transaction: "
                    << "SetHook sfHookParameter contains object other than sfHookParameterName/Value.";
                return false;
            }

            if (name == sfHookParameterName)
                nameFound = true;
        }

        if (!nameFound)
        {
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::PARAMETERS_NAME << ")[" << HS_ACC()
                << "]: Malformed transaction: "
                << "SetHook sfHookParameter must contain at least sfHookParameterName";
            return false;
        }
    }

    return true;
}

// infer which operation the user is attempting to execute from the present and absent fields
HookSetOperation inferOperation(STObject const& hookSetObj)
{
    uint64_t wasmByteCount = hookSetObj.isFieldPresent(sfCreateCode) ? 
            hookSetObj.getFieldVL(sfCreateCode).size() : 0;
    bool hasHash = hookSetObj.isFieldPresent(sfHookHash);
    bool hasCode = hookSetObj.isFieldPresent(sfCreateCode);


    if (hasHash && hasCode)        // Both HookHash and CreateCode: invalid
        return hsoINVALID;
    else if (hasHash)        // Hookhash only: install
        return hsoINSTALL;
    else if (hasCode)        // CreateCode only: either delete or create
        return wasmByteCount > 0 ? hsoCREATE : hsoDELETE;
    else if (
        !hasHash && !hasCode &&
        !hookSetObj.isFieldPresent(sfHookGrants) &&
        !hookSetObj.isFieldPresent(sfHookNamespace) &&
        !hookSetObj.isFieldPresent(sfHookParameters) &&
        !hookSetObj.isFieldPresent(sfHookOn) &&
        !hookSetObj.isFieldPresent(sfHookApiVersion) &&
        !hookSetObj.isFieldPresent(sfFlags))
        return hsoNOOP;
    
    return hookSetObj.isFieldPresent(sfHookNamespace) ? hsoNSDELETE : hsoUPDATE;

}


// may throw overflow_error
std::pair<bool, uint64_t>
validateCreateCode(SetHookCtx& ctx, STObject const& hookSetObj)
{

    if (!hookSetObj.isFieldPresent(sfCreateCode))
        return { false, 0 };

    uint64_t maxInstrCount = 0;
    Blob hook = hookSetObj.getFieldVL(sfCreateCode);
    uint64_t byteCount = hook.size();

    // RH TODO compute actual smallest possible hook and update this value
    if (byteCount < 10)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC() << "]: "
            << "Malformed transaction: Hook was not valid webassembly binary. Too small.";
        return {false, 0};
    }

    // check header, magic number
    unsigned char header[8] = { 0x00U, 0x61U, 0x73U, 0x6DU, 0x01U, 0x00U, 0x00U, 0x00U };
    for (int i = 0; i < 8; ++i)
    {
        if (hook[i] != header[i])
        {
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC() << "]: "
                << "Malformed transaction: Hook was not valid webassembly binary. "
                << "Missing magic number or version.";
            return {false, 0};
        }
    }

    // now we check for guards... first check if _g is imported
    int guard_import_number = -1;
    int last_import_number = -1;
    for (int i = 8, j = 0; i < hook.size();)
    {

        if (j == i)
        {
            // if the loop iterates twice with the same value for i then
            // it's an infinite loop edge case
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC() << "]: Malformed transaction: Hook is invalid WASM binary.";
            return {false, 0};
        }

        j = i;

        // each web assembly section begins with a single byte section type followed by an leb128 length
        int section_type = hook[i++];
        int section_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
        //int section_start = i;

        if (DEBUG_GUARD_CHECK)
            printf("WASM binary analysis -- upto %d: section %d with length %d\n",
                    i, section_type, section_length);

        int next_section = i + section_length;

        // we are interested in the import section... we need to know if _g is imported and which import# it is
        if (section_type == 2) // import section
        {
            int import_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (import_count <= 0)
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not import any functions... "
                    << "required at least guard(uint32_t, uint32_t) and accept, reject or rollback";
                return {false, 0};
            }

            // process each import one by one
            int func_upto = 0; // not all imports are functions so we need an indep counter for these
            for (int j = 0; j < import_count; ++j)
            {
                // first check module name
                int mod_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (mod_length < 1 || mod_length > (hook.size() - i))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to specify nil or invalid import module";
                    return {false, 0};
                }

                if (std::string_view( (const char*)(hook.data() + i), (size_t)mod_length ) != "env")
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to specify import module other than 'env'";
                    return {false, 0};
                }

                i += mod_length; CHECK_SHORT_HOOK();

                // next get import name
                int name_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (name_length < 1 || name_length > (hook.size() - i))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to specify nil or invalid import name";
                    return {false, 0};
                }

                std::string import_name { (const char*)(hook.data() + i), (size_t)name_length };

                i += name_length; CHECK_SHORT_HOOK();

                // next get import type
                if (hook[i] > 0x00)
                {
                    // not a function import
                    // RH TODO check these other imports for weird stuff
                    i++; CHECK_SHORT_HOOK();
                    parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                    continue;
                }

                // execution to here means it's a function import
                i++; CHECK_SHORT_HOOK();
                /*int type_idx = */
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();

                // RH TODO: validate that the parameters of the imported functions are correct
                if (import_name == "_g")
                {
                    guard_import_number = func_upto;
                } else if (hook_api::import_whitelist.find(import_name) == hook_api::import_whitelist.end())
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to import a function that does not "
                        << "appear in the hook_api function set: `" << import_name << "`";
                    return {false, 0};
                }
                func_upto++;
            }

            if (guard_import_number == -1)
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not import _g (guard) function";
                return {false, 0};
            }

            last_import_number = func_upto - 1;

            // we have an imported guard function, so now we need to enforce the guard rules
            // which are:
            // 1. all functions must start with a guard call before any branching [ RH TODO ]
            // 2. all loops must start with a guard call before any branching
            // to enforce these rules we must do a second pass of the wasm in case the function
            // section was placed in this wasm binary before the import section

        } else
        if (section_type == 7) // export section
        {
            int export_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (export_count <= 0)
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not export any functions... "
                    << "required hook(int64_t), callback(int64_t).";
                return {false, 0};
            }

            bool found_hook_export = false;
            bool found_cbak_export = false;
            for (int j = 0; j < export_count && !(found_hook_export && found_cbak_export); ++j)
            {
                int name_len = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (name_len == 4)
                {

                    if (hook[i] == 'h' && hook[i+1] == 'o' && hook[i+2] == 'o' && hook[i+3] == 'k')
                        found_hook_export = true;
                    else
                    if (hook[i] == 'c' && hook[i+1] == 'b' && hook[i+2] == 'a' && hook[i+3] == 'k')
                        found_cbak_export = true;
                }

                i += name_len + 1;
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            }

            // execution to here means export section was parsed
            if (!(found_hook_export && found_cbak_export))
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not export: " <<
                    ( !found_hook_export ? "hook(int64_t); " : "" ) <<
                    ( !found_cbak_export ? "cbak(int64_t);"  : "" );
                return {false, 0};
            }
        }

        i = next_section;
        continue;
    }


    // second pass... where we check all the guard function calls follow the guard rules
    // minimal other validation in this pass because first pass caught most of it
    for (int i = 8; i < hook.size();)
    {

        int section_type = hook[i++];
        int section_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
        //int section_start = i;
        int next_section = i + section_length;

        // RH TODO: parse anywhere else an expr is allowed in wasm and enforce rules there too
        if (section_type == 10) // code section
        {
            // these are the functions
            int func_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();

            for (int j = 0; j < func_count; ++j)
            {
                // parse locals
                int code_size = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                int code_end = i + code_size;
                int local_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                for (int k = 0; k < local_count; ++k)
                {
                    /*int array_size = */
                    parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                    if (!(hook[i] >= 0x7C && hook[i] <= 0x7F))
                    {
                        JLOG(ctx.j.trace())
                            << "HookSet[" << HS_ACC() << "]: Invalid local type. "
                            << "Codesec: " << j << " "
                            << "Local: " << k << " "
                            << "Offset: " << i;
                        return {false, 0};
                    }
                    i++; CHECK_SHORT_HOOK();
                }

                if (i == code_end)
                    continue; // allow empty functions

                // execution to here means we are up to the actual expr for the codesec/function

                auto [valid, instruction_count] =
                    check_guard(ctx, hook, j, i, code_end, guard_import_number, last_import_number);

                if (!valid)
                    return {false, 0};

                // the worst case execution is the fee, this includes the worst case between cbak and hook
                if (instruction_count > maxInstrCount)
                    maxInstrCount = instruction_count;

                i = code_end;

            }
        }
        i = next_section;
    }

    // execution to here means guards are installed correctly

    JLOG(ctx.j.trace())
        << "HookSet[" << HS_ACC() << "]: Trying to wasm instantiate proposed hook "
        << "size = " <<  hook.size();

    std::optional<std::string> result = 
        hook::HookExecutor::validateWasm(hook.data(), (size_t)hook.size());

    if (result)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC() << "]: "
            << "Tried to set a hook with invalid code. VM error: " << *result;
        return {false, 0};
    }

    return {true, maxInstrCount};
}

// This is a context-free validation, it does not take into account the current state of the ledger
// returns  < valid, instruction count >
// may throw overflow_error
std::pair<bool, uint64_t>
validateHookSetEntry(SetHookCtx& ctx, STObject const& hookSetObj)
{

    uint32_t flags = hookSetObj.isFieldPresent(sfFlags) ? hookSetObj.getFieldU32(sfFlags) : 0;

    switch (inferOperation(hookSetObj))
    {
        case hsoNOOP:
        {
            return {true, 0};
        }

        case hsoNSDELETE:
        {
            // namespace delete operation
            if (hookSetObj.isFieldPresent(sfHookGrants)         ||
                hookSetObj.isFieldPresent(sfHookParameters)     ||
                hookSetObj.isFieldPresent(sfHookOn)             ||
                hookSetObj.isFieldPresent(sfHookApiVersion)     ||
                !hookSetObj.isFieldPresent(sfFlags)             ||
                !hookSetObj.isFieldPresent(sfHookNamespace))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::NSDELETE_FIELD << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook nsdelete operation should contain only "
                    << "sfHookNamespace & sfFlags";
                return {false, 0};
            }

            if (flags != hsfNSDELETE)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::NSDELETE_FLAGS << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook nsdelete operation should only specify hsfNSDELETE";
                return {false, 0};
            }

            return {true, 0};
        }

        case hsoDELETE:
        {
            if (hookSetObj.isFieldPresent(sfHookGrants)     ||
                hookSetObj.isFieldPresent(sfHookParameters) ||
                hookSetObj.isFieldPresent(sfHookOn)         ||
                hookSetObj.isFieldPresent(sfHookApiVersion) ||
                hookSetObj.isFieldPresent(sfHookNamespace)  ||
                !hookSetObj.isFieldPresent(sfFlags))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::DELETE_FIELD << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook delete operation should contain only sfCreateCode & sfFlags";
                return {false, 0};
            }

            if (!(flags & hsfOVERRIDE))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::OVERRIDE_MISSING << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook delete operation was missing the hsfOVERRIDE flag";
                return {false, 0};
            }


            if (flags & ~(hsfOVERRIDE | hsfNSDELETE))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::FLAGS_INVALID << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook delete operation specified invalid flags";
                return {false, 0};
            }

            return {true, 0};
        }

        case hsoINSTALL:
        {
            // validate hook params structure, if any
            if (hookSetObj.isFieldPresent(sfHookParameters) &&
                !validateHookParams(ctx, hookSetObj.getFieldArray(sfHookParameters)))
                return {false, 0};

            // validate hook grants structure, if any
            if (hookSetObj.isFieldPresent(sfHookGrants) &&
                !validateHookGrants(ctx, hookSetObj.getFieldArray(sfHookGrants)))
                return {false, 0};
            
            // api version not allowed in update
            if (hookSetObj.isFieldPresent(sfHookApiVersion))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::API_ILLEGAL << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook install operation sfHookApiVersion must not be included.";
                return {false, 0};
            }
    
            // namespace may be valid, if the user so chooses
            // hookon may be present if the user so chooses
            // flags may be present if the user so chooses

            return {true, 0};
        }

        case hsoUPDATE:
        {
            // must not specify override flag
            if ((flags & hsfOVERRIDE) || 
                ((flags & hsfNSDELETE) && !hookSetObj.isFieldPresent(sfHookNamespace)))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::FLAGS_INVALID << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook update operation only hsfNSDELETE may be specified and "
                    << "only if a new HookNamespace is also specified.";
                return {false, 0};
            }

            // validate hook params structure
            if (hookSetObj.isFieldPresent(sfHookParameters) &&
                !validateHookParams(ctx, hookSetObj.getFieldArray(sfHookParameters)))
                return {false, 0};

            // validate hook grants structure
            if (hookSetObj.isFieldPresent(sfHookGrants) &&
                !validateHookGrants(ctx, hookSetObj.getFieldArray(sfHookGrants)))
                return {false, 0};
            
            // api version not allowed in update
            if (hookSetObj.isFieldPresent(sfHookApiVersion))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::API_ILLEGAL << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook update operation sfHookApiVersion must not be included.";
                return {false, 0};
            }

            // namespace may be valid, if the user so chooses
            // hookon may be present if the user so chooses
            // flags may be present if the user so chooses

            return {true, 0};
        }

        case hsoCREATE:
        {
            // validate hook params structure
            if (hookSetObj.isFieldPresent(sfHookParameters) &&
                !validateHookParams(ctx, hookSetObj.getFieldArray(sfHookParameters)))
                return {false, 0};

            // validate hook grants structure
            if (hookSetObj.isFieldPresent(sfHookGrants) &&
                !validateHookGrants(ctx, hookSetObj.getFieldArray(sfHookGrants)))
                return {false, 0};


            // ensure hooknamespace is present
            if (!hookSetObj.isFieldPresent(sfHookNamespace))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::NAMESPACE_MISSING << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHookDefinition must contain sfHookNamespace.";
                return {false, 0};
            }

            // validate api version, if provided
            if (!hookSetObj.isFieldPresent(sfHookApiVersion))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::API_MISSING << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHookApiVersion must be included.";
                return {false, 0};
            }
                
            auto version = hookSetObj.getFieldU16(sfHookApiVersion);
            if (version != 0)
            {
                // we currently only accept api version 0
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::API_INVALID << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHook->sfHookApiVersion invalid. (Try 0).";
                return {false, 0};
            }

            // validate sfHookOn
            if (!hookSetObj.isFieldPresent(sfHookOn))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::HOOKON_MISSING << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook must include sfHookOn when creating a new hook.";
                return {false, 0};
            }
            
            // finally validate web assembly byte code
            return validateCreateCode(ctx, hookSetObj);            
        }
        
        case hsoINVALID:
        default:
        {
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::HASH_OR_CODE << ")[" << HS_ACC()
                << "]: Malformed transaction: SetHook must provide only one of sfCreateCode or sfHookHash.";
            return {false, 0};
        }
    }
}

FeeUnit64
SetHook::calculateBaseFee(ReadView const& view, STTx const& tx)
{
    FeeUnit64 extraFee{0};

    auto const& hookSets = tx.getFieldArray(sfHooks);

    for (auto const& hookSet : hookSets)
    {
        auto const& hookSetObj = dynamic_cast<STObject const*>(&hookSet);

        if (!hookSetObj->isFieldPresent(sfCreateCode))
            continue;

        extraFee += FeeUnit64{
            hook::computeCreationFee(
                hookSetObj->getFieldVL(sfCreateCode).size())};
    }

    return Transactor::calculateBaseFee(view, tx) + extraFee;
}

TER
SetHook::preclaim(ripple::PreclaimContext const& ctx)
{

    auto const& hookSets = ctx.tx.getFieldArray(sfHooks);

    for (auto const& hookSet : hookSets)
    {

        auto const& hookSetObj = dynamic_cast<STObject const*>(&hookSet);

        if (!hookSetObj->isFieldPresent(sfHookHash))
            continue;

        auto const& hash = hookSetObj->getFieldH256(sfHookHash);
        {
            if (!ctx.view.exists(keylet::hookDefinition(hash)))
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: No hook exists with the specified hash.";
                return terNO_HOOK;
            }
        }
    }

    return tesSUCCESS;
}

NotTEC
SetHook::preflight(PreflightContext const& ctx)
{

    if (!ctx.rules.enabled(featureHooks))
    {
        JLOG(ctx.j.warn()) << "HookSet[" << HS_ACC() << "]: Hooks Amendment not enabled!";
        return temDISABLED;
    }

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    if (!ctx.tx.isFieldPresent(sfHooks))
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC() << "]: Malformed transaction: SetHook lacked sfHooks array.";
        return temMALFORMED;
    }

    auto const& hookSets = ctx.tx.getFieldArray(sfHooks);

    if (hookSets.size() < 1)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHooks empty.";
        return temMALFORMED;
    }

    if (hookSets.size() > hook::maxHookChainLength())
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHooks contains more than " << hook::maxHookChainLength()
            << " entries.";
        return temMALFORMED;
    }

    SetHookCtx shCtx
    {
       .j = ctx.j,
       .tx = ctx.tx,
       .app = ctx.app
    };

    bool allBlank = true;

    for (auto const& hookSet : hookSets)
    {

        auto const& hookSetObj = dynamic_cast<STObject const*>(&hookSet);

        if (!hookSetObj || (hookSetObj->getFName() != sfHook))
        {
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: SetHook sfHooks contains obj other than sfHook.";
            return temMALFORMED;
        }

        if (hookSetObj->getCount() == 0) // skip blanks
            continue;

        allBlank = false;

        for (auto const& hookSetElement : *hookSetObj)
        {
            auto const& name = hookSetElement.getFName();

            if (name != sfCreateCode &&
                name != sfHookHash &&
                name != sfHookNamespace &&
                name != sfHookParameters &&
                name != sfHookOn &&
                name != sfHookGrants &&
                name != sfHookApiVersion &&
                name != sfFlags)
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHook contains invalid field.";
                return temMALFORMED;
            }
        }

        try
        {

            // may throw if leb128 overflow is detected
            [[maybe_unused]]
            auto [valid,  _] =
                validateHookSetEntry(shCtx, *hookSetObj);

            if (!valid)
                    return temMALFORMED;
        }
        catch (std::exception& e)
        {
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC() << "]: Exception: " << e.what();
            return temMALFORMED;
        }
    }

    if (allBlank)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHooks must contain at least one non-blank sfHook.";
        return temMALFORMED;
    }


    return preflight2(ctx);
}



TER
SetHook::doApply()
{
    preCompute();
    return setHook();
}

void
SetHook::preCompute()
{
    return Transactor::preCompute();
}

TER
SetHook::destroyNamespace(
    SetHookCtx& ctx,
    ApplyView& view,
    const AccountID& account,
    const Keylet & dirKeylet        // the keylet of the namespace directory
) {
    JLOG(ctx.j.trace())
        << "HookSet[" << HS_ACC() << "]: DeleteState "
        << "Destroying Hook Namespace for " << account << " namespace keylet " << dirKeylet.key;

    std::shared_ptr<SLE const> sleDirNode{};
    unsigned int uDirEntry{0};
    uint256 dirEntry{beast::zero};

    if (dirIsEmpty(view, dirKeylet))
        return tesSUCCESS;

    auto sleAccount = view.peek(keylet::account(account));
    if (!sleAccount)
    {
        JLOG(ctx.j.fatal())
            << "HookSet[" << HS_ACC() << "]: Account does not exist to destroy namespace from";
        return tefBAD_LEDGER;
    }


    if (!cdirFirst(
            view,
            dirKeylet.key,
            sleDirNode,
            uDirEntry,
            dirEntry)) {
            JLOG(ctx.j.fatal())
                << "HookSet[" << HS_ACC() << "]: DeleteState "
                << "account directory missing ";
        return tefINTERNAL;
    }

    uint32_t stateCount =sleAccount->getFieldU32(sfHookStateCount);
    uint32_t oldStateCount = stateCount;

    do
    {
        // Make sure any directory node types that we find are the kind
        // we can delete.
        Keylet const itemKeylet{ltCHILD, dirEntry};
        auto sleItem = view.peek(itemKeylet);
        if (!sleItem)
        {
            // Directory node has an invalid index.  Bail out.
            JLOG(ctx.j.fatal())
                << "HookSet[" << HS_ACC() << "]: DeleteState "
                << "directory node in ledger " << view.seq() << " "
                << "has index to object that is missing: "
                << to_string(dirEntry);
            return tefBAD_LEDGER;
        }

        auto nodeType = sleItem->getFieldU16(sfLedgerEntryType);

        if (nodeType == ltHOOK_STATE) {
            // delete it!
            auto const hint = (*sleItem)[sfOwnerNode];
            if (!view.dirRemove(dirKeylet, hint, itemKeylet.key, false))
            {
                JLOG(ctx.j.fatal())
                    << "HookSet[" << HS_ACC() << "]: DeleteState "
                    << "directory node in ledger " << view.seq() << " "
                    << "has undeletable ltHOOK_STATE";
                return tefBAD_LEDGER;
            }
            view.erase(sleItem);
            stateCount--;
        }


    } while (cdirNext(view, dirKeylet.key, sleDirNode, uDirEntry, dirEntry));

    if (stateCount > oldStateCount)
    {
        JLOG(ctx.j.fatal())
            << "HookSet[" << HS_ACC() << "]: DeleteState "
            << "stateCount less than zero (overflow)";

        return tefBAD_LEDGER;
    }

    sleAccount->setFieldU32(sfHookStateCount, stateCount);

    return tesSUCCESS;
}


// returns true if the reference counted ledger entry should be marked for deletion
// i.e. it has a zero reference count after the decrement is completed
// otherwise returns false (but still decrements reference count)
bool reduceReferenceCount(std::shared_ptr<STLedgerEntry>& sle)
{
    if (sle && sle->isFieldPresent(sfReferenceCount))
    {
        // reduce reference count on reference counted object
        uint64_t refCount = sle->getFieldU64(sfReferenceCount);
        if (refCount > 0)
        {
            refCount--;
            sle->setFieldU64(sfReferenceCount, refCount);
        }

        return refCount <= 0;
    }
    return false;
}

void incrementReferenceCount(std::shared_ptr<STLedgerEntry>& sle)
{
    if (sle && sle->isFieldPresent(sfReferenceCount))
        sle->setFieldU64(sfReferenceCount, sle->getFieldU64(sfReferenceCount) + 1);
}

// increment or create namespace directory
/*
TER
SetHook::
createOrReuseNamespace(ripple::Keylet& newDirKeylet)
{
    auto& sle = view().peek(newDirKeylet);
    if (sle)
    {
        incrementReferenceCount(sle);
        return tesSUCCESS;
    }

    sle = std::make_shared<SLE>(newDirKeylet);
    
    auto const page = view().dirInsert(
        keylet::ownerDir(account_),
        newDirKeylet->key,
        describeOwnerDir(account_));
    JLOG(ctx.j.trace()) << "Create state dir for account " << toBase58(account_)
                     << ": " << (page ? "success" : "failure");
    if (!page)
        return tecDIR_FULL;
    sle->setFieldU64(sfOwnerNode, *page);
    sle->setFieldU64(sfReferenceCount, 1);
    view().insert(sle);
    return tesSUSCCESS;
}
        //if (oldDirSLE)
        //    reduceReferenceCount(oldDirSLE, (flag & hsoNSDELETE) ? dirsToDestroy : std::nullopt);
*/

TER
updateHookParameters(
        SetHookCtx& ctx,
        ripple::STObject const& hookSetObj,
        std::shared_ptr<STLedgerEntry>& oldDefSLE,
        ripple::STObject& newHook)
{
    const int  paramKeyMax = hook::maxHookParameterKeySize();
    const int  paramValueMax = hook::maxHookParameterValueSize();
    
    std::map<ripple::Blob, ripple::Blob> parameters;

    // first pull the parameters into a map
    auto const& hookParameters = hookSetObj.getFieldArray(sfHookParameters);
    for (auto const& hookParameter : hookParameters)
    {
        auto const& hookParameterObj = dynamic_cast<STObject const*>(&hookParameter);
        parameters[hookParameterObj->getFieldVL(sfHookParameterName)] =
            hookParameterObj->getFieldVL(sfHookParameterValue);
    }

    // then erase anything that is the same as the definition's default parameters
    if (parameters.size() > 0)
    {
        auto const& defParameters = oldDefSLE->getFieldArray(sfHookParameters);
        for (auto const& hookParameter : defParameters)
        {
            auto const& hookParameterObj = dynamic_cast<STObject const*>(&hookParameter);
            ripple::Blob n = hookParameterObj->getFieldVL(sfHookParameterName);
            ripple::Blob v = hookParameterObj->getFieldVL(sfHookParameterValue);

            if (parameters.find(n) != parameters.end() && parameters[n] == v)
                parameters.erase(n);
        }
    }

    int parameterCount = (int)(parameters.size());
    if (parameterCount > 16)
    {
        JLOG(ctx.j.fatal())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: Txn would result in too many parameters on hook";
        return tecINTERNAL;
    }

    STArray newParameters {sfHookParameters, parameterCount};
    for (const auto& [parameterName, parameterValue] : parameters)
    {
        if (parameterName.size() > paramKeyMax || parameterValue.size() > paramValueMax)
        {
            JLOG(ctx.j.fatal())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: Txn would result in a too large parameter name/value on hook";
            return tecINTERNAL;
        }

        STObject param { sfHookParameter };
        param.setFieldVL(sfHookParameterName, parameterName);
        param.setFieldVL(sfHookParameterValue, parameterValue);
        newParameters.push_back(std::move(param));
    }

    if (newParameters.size() > 0)
        newHook.setFieldArray(sfHookParameters, std::move(newParameters));

    return tesSUCCESS;
}


struct KeyletComparator
{
    bool operator()(const Keylet& lhs, const Keylet& rhs) const
    { 
        return lhs.type < rhs.type || (lhs.type == rhs.type && lhs.key < rhs.key);
    }
};

TER
SetHook::setHook()
{

    /**
     * Each account has optionally an ltHOOK object
     * Which contains an array (sfHooks) of sfHook objects
     * The set hook transaction also contains an array (sfHooks) of sfHook objects
     * These two arrays are mapped 1-1 when updating, inserting or deleting hooks
     * When the user submits a new hook that does not yet exist on the ledger an ltHOOK_DEFINITION object is created
     * Further users setting the same hook code will reference this object using sfHookHash.
     */

    SetHookCtx ctx
    {
        .j = ctx_.app.journal("View"),
        .tx = ctx_.tx,
        .app = ctx_.app
    };

    const int  blobMax = hook::maxHookWasmSize();
    auto const accountKeylet = keylet::account(account_);
    auto const hookKeylet = keylet::hook(account_);

    ripple::STArray newHooks{sfHooks, 8};
    auto newHookSLE = std::make_shared<SLE>(hookKeylet);

    int oldHookCount = 0;
    std::optional<std::reference_wrapper<ripple::STArray const>> oldHooks;
    auto const& oldHookSLE = view().peek(hookKeylet);

    if (oldHookSLE)
    {
       oldHooks = oldHookSLE->getFieldArray(sfHooks);
       oldHookCount = (oldHooks->get()).size();
    }

    std::set<ripple::Keylet, KeyletComparator> defsToDestroy {};
    std::set<ripple::Keylet, KeyletComparator> dirsToDestroy {};

    int hookSetNumber = -1;
    auto const& hookSets = ctx.tx.getFieldArray(sfHooks);

    for (auto const& hookSet : hookSets)
    {
        hookSetNumber++;

        ripple::STObject                                                newHook         { sfHook };
        std::optional<std::reference_wrapper<ripple::STObject const>>   oldHook;
        // an existing hook would only be present if the array slot also exists on the ltHOOK object
        if (hookSetNumber < oldHookCount)
            oldHook = std::cref((oldHooks->get()[hookSetNumber]).downcast<ripple::STObject const>());

        STObject const* hookSetObj        = dynamic_cast<STObject const*>(&hookSet);

        std::optional<ripple::uint256>                                  oldNamespace;
        std::optional<ripple::uint256>                                  defNamespace;
        std::optional<ripple::Keylet>                                   oldDirKeylet;
        std::optional<ripple::Keylet>                                   oldDefKeylet;
        std::optional<ripple::Keylet>                                   newDefKeylet;
        std::shared_ptr<STLedgerEntry>                                  oldDefSLE;
        std::shared_ptr<STLedgerEntry>                                  newDefSLE;
        std::shared_ptr<STLedgerEntry>                                  oldDirSLE;

        std::optional<ripple::uint256>                                  newNamespace;
        std::optional<ripple::Keylet>                                   newDirKeylet;

        std::optional<uint64_t>                                         oldHookOn;
        std::optional<uint64_t>                                         newHookOn;
        std::optional<uint64_t>                                         defHookOn;

        // when hsoCREATE is invoked it populates this variable in case the hook definition already exists
        // and the operation falls through into a hsoINSTALL operation instead
        std::optional<ripple::uint256>                                  createHookHash;
        /**
         * This is the primary HookSet loop. We iterate the sfHooks array inside the txn
         * each entry of this array is available as hookSetObj.
         * Depending on whether or not an existing hook is present in the array slot we are currently up to
         * this hook and its various attributes are available in the optionals prefixed with old.
         * Even if an existing hook is being modified by the sethook obj, we create a newHook obj
         * so a degree of copying is required.
         */

        uint32_t flags = hookSetObj->isFieldPresent(sfFlags) ? hookSetObj->getFieldU32(sfFlags) : 0;

        HookSetOperation op = inferOperation(*hookSetObj);


        // if an existing hook exists at this position in the chain then extract the relevant fields
        if (oldHook && oldHook->get().isFieldPresent(sfHookHash))
        {
            oldDefKeylet = keylet::hookDefinition(oldHook->get().getFieldH256(sfHookHash));
            oldDefSLE = view().peek(*oldDefKeylet);
            if (oldDefSLE)
                defNamespace = oldDefSLE->getFieldH256(sfHookNamespace);

            if (oldHook->get().isFieldPresent(sfHookNamespace))
                oldNamespace = oldHook->get().getFieldH256(sfHookNamespace);
            else if (defNamespace)
                oldNamespace = *defNamespace;

            oldDirKeylet = keylet::hookStateDir(account_, *oldNamespace);
            oldDirSLE = view().peek(*oldDirKeylet);
            if (oldDefSLE)
                defHookOn = oldDefSLE->getFieldU64(sfHookOn);

            if (oldHook->get().isFieldPresent(sfHookOn))
                oldHookOn = oldHook->get().getFieldU64(sfHookOn);
            else if (defHookOn)
                oldHookOn = *defHookOn;
        }

        // in preparation for three way merge populate fields if they are present on the HookSetObj
        if (hookSetObj->isFieldPresent(sfHookHash))
        {
            newDefKeylet = keylet::hookDefinition(hookSetObj->getFieldH256(sfHookHash));
            newDefSLE = view().peek(*newDefKeylet);
        }

        if (hookSetObj->isFieldPresent(sfHookOn))
            newHookOn = hookSetObj->getFieldU64(sfHookOn);

        if (hookSetObj->isFieldPresent(sfHookNamespace))
        {
            newNamespace = hookSetObj->getFieldH256(sfHookNamespace);
            newDirKeylet = keylet::hookStateDir(account_, *newNamespace);
        }


        // users may destroy a namespace in any operation except NOOP and INVALID
        if (op != hsoNOOP && op != hsoINVALID && (flags & hsoNSDELETE) && oldDirSLE)
        {
            if (op == hsoNSDELETE && newDirKeylet)
                dirsToDestroy.emplace(*newDirKeylet);
            else if (oldDirKeylet)
                dirsToDestroy.emplace(*oldDirKeylet);
            else
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: SetHook hsoNSDELETE specified but nothing to delete";
            }
        }

        switch (op)
        {
            case hsoNSDELETE:
            {
                // this case is handled directly above already
                continue;
            }
            
            case hsoNOOP:
            {
                // if a hook already exists here then migrate it to the new array
                // if it doesn't exist just place a blank object here
                newHooks.push_back( oldHook ? oldHook->get() : ripple::STObject{sfHook} );
                continue;
            }
            
            case hsoDELETE:
            {

                if (!(flags & hsfOVERRIDE))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC()
                        << "]: SetHook delete operation requires hsfOVERRIDE flag";
                    return tecREQUIRES_FLAG;
                }
               
                // place an empty corresponding Hook 
                newHooks.push_back(ripple::STObject{sfHook});

                if (!oldHook)
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC()
                        << "]: SetHook delete operation deletes non-existent hook";

                    continue;
                }

                // decrement the hook definition and mark it for deletion if appropriate
                if (oldDefSLE)
                {
                    if (reduceReferenceCount(oldDefSLE))
                        defsToDestroy.emplace(*oldDefKeylet);

                    view().update(oldDefSLE);
                }

                continue;
            }

            case hsoUPDATE:
            {
                // set the namespace if it differs from the definition namespace
                if (newNamespace && *defNamespace != *newNamespace)
                    newHook.setFieldH256(sfHookNamespace, *newNamespace);

                // set the hookon field if it differs from definition
                if (newHookOn && *defHookOn != *newHookOn)
                    newHook.setFieldU64(sfHookOn, *newHookOn);

                // parameters
                TER result = 
                    updateHookParameters(ctx, *hookSetObj, oldDefSLE, newHook);

                if (result != tesSUCCESS)
                    return result;

                // if grants are provided set them
                if (hookSetObj->isFieldPresent(sfHookGrants))
                    newHook.setFieldArray(sfHookGrants, hookSetObj->getFieldArray(sfHookGrants));

                newHooks.push_back(std::move(newHook));
                continue;
            }


            case hsoCREATE:
            {
                if (oldHook && oldHook->get().isFieldPresent(sfHookHash) && !(flags & hsfOVERRIDE))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC()
                        << "]: SetHook create operation would override but hsfOVERRIDE flag wasn't specified";
                    return tecREQUIRES_FLAG;
                }
                

                ripple::Blob wasmBytes = hookSetObj->getFieldVL(sfCreateCode);

                if (wasmBytes.size() > blobMax)
                {
                    JLOG(ctx.j.warn())
                        << "HookSet[" << HS_ACC()
                        << "]: Malformed transaction: SetHook operation would create blob larger than max";
                    return tecINTERNAL;
                }

                createHookHash = ripple::sha512Half_s(
                    ripple::Slice(wasmBytes.data(), wasmBytes.size())
                );

                auto keylet = ripple::keylet::hookDefinition(*createHookHash);


                if (view().exists(keylet))
                {
                    // update reference count
                    newDefSLE = view().peek(keylet);
                    newDefKeylet = keylet;
        
                    // this falls through to hsoINSTALL
                }
                else
                {
                    uint64_t maxInstrCount = 0;
                    bool valid = false;

                    // create hook definition SLE
                    try
                    {

                        std::tie(valid, maxInstrCount) =
                            validateHookSetEntry(ctx, *hookSetObj);

                        if (!valid)
                        {
                            JLOG(ctx.j.warn())
                                << "HookSet[" << HS_ACC()
                                << "]: Malformed transaction: SetHook operation would create invalid hook wasm";
                            return tecINTERNAL;
                        }
                    }
                    catch (std::exception& e)
                    {
                        JLOG(ctx.j.warn())
                            << "HookSet[" << HS_ACC()
                            << "]: Malformed transaction: SetHook operation would create invalid hook wasm";
                        return tecINTERNAL;
                    }
                        
                    // decrement the hook definition and mark it for deletion if appropriate
                    if (oldDefSLE)
                    {
                        if (reduceReferenceCount(oldDefSLE))
                            defsToDestroy.emplace(*oldDefKeylet);

                        view().update(oldDefSLE);
                    }

                    auto newHookDef = std::make_shared<SLE>( keylet );
                    newHookDef->setFieldH256(sfHookHash, *createHookHash);
                    newHookDef->setFieldU64(    sfHookOn, *newHookOn);
                    newHookDef->setFieldH256(   sfHookNamespace, *newNamespace);
                    newHookDef->setFieldArray(  sfHookParameters,
                            hookSetObj->isFieldPresent(sfHookParameters)
                            ? hookSetObj->getFieldArray(sfHookParameters)
                            : STArray {} );
                    newHookDef->setFieldU16(    sfHookApiVersion, hookSetObj->getFieldU16(sfHookApiVersion));
                    newHookDef->setFieldVL(     sfCreateCode, wasmBytes);
                    newHookDef->setFieldH256(   sfHookSetTxnID, ctx.tx.getTransactionID());
                    newHookDef->setFieldU64(    sfReferenceCount, 1);
                    newHookDef->setFieldAmount(sfFee,  XRPAmount {hook::computeExecutionFee(maxInstrCount)} );
                    view().insert(newHookDef);
                    newHook.setFieldH256(sfHookHash, *createHookHash);
                    newHooks.push_back(std::move(newHook));
                    continue;
                }
                [[fallthrough]];
            }
        
            // the create operation above falls through to this install operation if the sfCreateCode that would
            // otherwise be created already exists on the ledger
            case hsoINSTALL:
            {
                if (oldHook && oldHook->get().isFieldPresent(sfHookHash) && !(flags & hsfOVERRIDE))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC()
                        << "]: SetHook install operation would override but hsfOVERRIDE flag wasn't specified";
                    return tecREQUIRES_FLAG;
                }

                // check if the target hook exists
                if (!newDefSLE)
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC()
                        << "]: SetHook install operation specified HookHash which does not exist on ledger";
                    return tecNO_ENTRY;
                }

                // decrement the hook definition and mark it for deletion if appropriate
                if (oldDefSLE)
                {
                    if (reduceReferenceCount(oldDefSLE))
                        defsToDestroy.emplace(*oldDefKeylet);

                    view().update(oldDefSLE);
                }

                // set the hookhash on the new hook, and allow for a fall through event from hsoCREATE
                if (!createHookHash)
                    createHookHash = hookSetObj->getFieldH256(sfHookHash);

                newHook.setFieldH256(sfHookHash, *createHookHash);

                // increment reference count of target HookDefintion
                incrementReferenceCount(newDefSLE);

                // change which definition we're using to the new target
                defNamespace = newDefSLE->getFieldH256(sfHookNamespace);
                defHookOn = newDefSLE->getFieldU64(sfHookOn);

                // set the namespace if it differs from the definition namespace
                if (newNamespace && *defNamespace != *newNamespace)
                    newHook.setFieldH256(sfHookNamespace, *newNamespace);

                // set the hookon field if it differs from definition
                if (newHookOn && *defHookOn != *newHookOn)
                    newHook.setFieldU64(sfHookOn, *newHookOn);

                // parameters
                TER result =
                    updateHookParameters(ctx, *hookSetObj, newDefSLE, newHook);

                if (result != tesSUCCESS)
                    return result;

                // if grants are provided set them
                if (hookSetObj->isFieldPresent(sfHookGrants))
                    newHook.setFieldArray(sfHookGrants, hookSetObj->getFieldArray(sfHookGrants));

                newHooks.push_back(std::move(newHook));

                view().update(newDefSLE);

                continue;
            }

            case hsoINVALID:
            default:
            {
                JLOG(ctx.j.warn())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: sethook could not find a namespace to place hook state into.";
                return tecCLAIM;
            }
        }
    }

    // clean up any Namespace directories marked for deletion and any zero reference Hook Definitions

    // dirs
    for (auto const& p : dirsToDestroy)
    {
        auto const& sle = view().peek(p);
        if (!sle)
            continue;
        destroyNamespace(ctx, view(), account_, p);
        view().erase(sle);
    }


    // defs
    for (auto const& p : defsToDestroy)
    {
        auto const& sle = view().peek(p);
        if (!sle)
            continue;
        uint64_t refCount = sle->getFieldU64(sfReferenceCount);
        if (refCount <= 0)
        {
            view().erase(sle);
        }
    }

    newHookSLE->setFieldArray(sfHooks, newHooks);
    newHookSLE->setAccountID(sfAccount, account_);

    if (oldHookSLE)
        view().erase(oldHookSLE);

    view().insert(newHookSLE);
    return tesSUCCESS;
}


}  // namespace ripple
