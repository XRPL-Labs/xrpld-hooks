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
#include <optional>
#include <variant>

#define DEBUG_GUARD_CHECK 1
#define HS_ACC() ctx.tx.getAccountID(sfAccount) << "-" << ctx.tx.getTransactionID()

namespace ripple {
//RH UPTO: sethook needs to correctly compute and charge fee for creating new hooks, updating existing hooks
//and it also needs to account for reserve requirements for namespaces, parameters and grants


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
            << "Malformed transaction: Hook truncated or otherwise invalid. "\
            << "SetHook.cpp:" << __LINE__;\
        return {};\
    }\
}

// checks the WASM binary for the appropriate required _g guard calls and rejects it if they are not found
// start_offset is where the codesection or expr under analysis begins and end_offset is where it ends
// returns {worst case instruction count} if valid or {} if invalid
// may throw overflow_error
std::optional<uint64_t>
check_guard(
        SetHookCtx& ctx,
        ripple::Blob& hook, int codesec,
        int start_offset, int end_offset, int guard_func_idx, int last_import_idx)
{

    if (DEBUG_GUARD_CHECK)
        printf("\ncheck_guard called with "
               "codesec=%d start_offset=%d end_offset=%d guard_func_idx=%d last_import_idx=%d\n",
               codesec, start_offset, end_offset, guard_func_idx, last_import_idx);

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

        CHECK_SHORT_HOOK();
        int instr = hook[i++];

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
                return {};
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
                        return {};
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
                        return {};
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
            return {};
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
                return {};
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
                return {};
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
                return {};
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
        return {};
    }

    // if we reach the end of the code looking for another trigger the guards are installed correctly
    if (mode == 1)
        return instruction_count[0].second;

    JLOG(ctx.j.trace())
        << "HookSet(" << hook::log::GUARD_MISSING << ")[" << HS_ACC() << "]: GuardCheck "
        << "Guard did not occur before end of loop / function. "
        << "Codesec: " << codesec;
    return {};
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
std::optional<  // unpopulated means invalid
std::pair<
    uint64_t,   // max instruction count for hook()
    uint64_t    // max instruction count for cbak()
>>
validateCreateCode(SetHookCtx& ctx, STObject const& hookSetObj)
{

    if (!hookSetObj.isFieldPresent(sfCreateCode))
        return {};

    uint64_t maxInstrCountHook = 0;
    uint64_t maxInstrCountCbak = 0;

    Blob hook = hookSetObj.getFieldVL(sfCreateCode);
    uint64_t byteCount = hook.size();

    // RH TODO compute actual smallest possible hook and update this value
    if (byteCount < 10)
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::WASM_TOO_SMALL << ")[" << HS_ACC() << "]: "
            << "Malformed transaction: Hook was not valid webassembly binary. Too small.";
        return {};
    }

    // check header, magic number
    unsigned char header[8] = { 0x00U, 0x61U, 0x73U, 0x6DU, 0x01U, 0x00U, 0x00U, 0x00U };
    for (int i = 0; i < 8; ++i)
    {
        if (hook[i] != header[i])
        {
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::WASM_BAD_MAGIC << ")[" << HS_ACC() << "]: "
                << "Malformed transaction: Hook was not valid webassembly binary. "
                << "Missing magic number or version.";
            return {};
        }
    }

    // these will store the function type indicies of hook and cbak if
    // hook and cbak are found in the export section
    std::optional<int> hook_func_idx;
    std::optional<int> cbak_func_idx;

    // this maps function ids to type ids, used for looking up the type of cbak and hook
    // as established inside the wasm binary.
    std::map<int, int> func_type_map;


    // now we check for guards... first check if _g is imported
    int guard_import_number = -1;
    int last_import_number = -1;
    int import_count = 0;
    for (int i = 8, j = 0; i < hook.size();)
    {

        if (j == i)
        {
            // if the loop iterates twice with the same value for i then
            // it's an infinite loop edge case
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::WASM_PARSE_LOOP << ")[" << HS_ACC() 
                << "]: Malformed transaction: Hook is invalid WASM binary.";
            return {};
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
            import_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (import_count <= 0)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::IMPORTS_MISSING << ")[" << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not import any functions... "
                    << "required at least guard(uint32_t, uint32_t) and accept or rollback";
                return {};
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
                        << "HookSet(" << hook::log::IMPORT_MODULE_BAD << ")[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to specify nil or invalid import module";
                    return {};
                }

                if (std::string_view( (const char*)(hook.data() + i), (size_t)mod_length ) != "env")
                {
                    JLOG(ctx.j.trace())
                        << "HookSet(" << hook::log::IMPORT_MODULE_ENV << ")[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to specify import module other than 'env'";
                    return {};
                }

                i += mod_length; CHECK_SHORT_HOOK();

                // next get import name
                int name_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (name_length < 1 || name_length > (hook.size() - i))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet(" << hook::log::IMPORT_NAME_BAD << ")[" 
                        << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to specify nil or invalid import name";
                    return {};
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
                        << "HookSet(" << hook::log::IMPORT_ILLEGAL << ")[" 
                        << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to import a function that does not "
                        << "appear in the hook_api function set: `" << import_name << "`";
                    return {};
                }
                func_upto++;
            }

            if (guard_import_number == -1)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::GUARD_IMPORT << ")[" << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not import _g (guard) function";
                return {};
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
                    << "HookSet(" << hook::log::EXPORTS_MISSING << ")["
                    << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not export any functions... "
                    << "required hook(int64_t), callback(int64_t).";
                return {};
            }

            for (int j = 0; j < export_count; ++j)
            {
                int name_len = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (name_len == 4)
                {

                    if (hook[i] == 'h' && hook[i+1] == 'o' && hook[i+2] == 'o' && hook[i+3] == 'k')
                    {
                        i += name_len; CHECK_SHORT_HOOK();
                        if (hook[i] != 0)
                        {
                            JLOG(ctx.j.trace())
                                << "HookSet(" << hook::log::EXPORT_HOOK_FUNC << ")["
                                << HS_ACC() << "]: Malformed transaction. "
                                << "Hook did not export: A valid int64_t hook(uint32_t)";
                            return {};
                        }

                        i++; CHECK_SHORT_HOOK();
                        hook_func_idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                        continue;
                    }
                    
                    if (hook[i] == 'c' && hook[i+1] == 'b' && hook[i+2] == 'a' && hook[i+3] == 'k')
                    {
                        i += name_len; CHECK_SHORT_HOOK();
                        if (hook[i] != 0)
                        {
                            JLOG(ctx.j.trace())
                                << "HookSet(" << hook::log::EXPORT_CBAK_FUNC << ")["
                                << HS_ACC() << "]: Malformed transaction. "
                                << "Hook did not export: A valid int64_t cbak(uint32_t)";
                            return {};
                        }
                        i++; CHECK_SHORT_HOOK();
                        cbak_func_idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                        continue;
                    }
                }

                i += name_len + 1;
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            }

            // execution to here means export section was parsed
            if (!hook_func_idx)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::EXPORT_MISSING << ")["
                    << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not export: "
                    << ( !hook_func_idx ? "int64_t hook(uint32_t); " : "" );
                return {};
            }
        }
        else if (section_type == 3) // function section
        {
            int function_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (function_count <= 0)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::FUNCS_MISSING 
                    << ")[" << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not establish any functions... "
                    << "required hook(int64_t), callback(int64_t).";
                return {};
            }

            for (int j = 0; j < function_count; ++j)
            {
                int type_idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                printf("Function map: func %d -> type %d\n", j, type_idx);
                func_type_map[j] = type_idx;
            }
        }

        i = next_section;
        continue;
    }

    // we must subtract import_count from the hook and cbak function in order to be able to 
    // look them up in the functions section. this is a rule of the webassembly spec
    // note that at this point in execution we are guarenteed these are populated
    *hook_func_idx -= import_count;

    if (cbak_func_idx)
        *cbak_func_idx -= import_count;

    if (func_type_map.find(*hook_func_idx) == func_type_map.end() ||
        (cbak_func_idx && func_type_map.find(*cbak_func_idx) == func_type_map.end()))
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::FUNC_TYPELESS << ")["
            << HS_ACC() << "]: Malformed transaction. "
            << "hook or cbak functions did not have a corresponding type in WASM binary.";
        return {};
    }

    int hook_type_idx = func_type_map[*hook_func_idx];

    // cbak function is optional so if it exists it has a type otherwise it is skipped in checks
    std::optional<int> cbak_type_idx;
    if (cbak_func_idx)
        cbak_type_idx = func_type_map[*cbak_func_idx];

    // second pass... where we check all the guard function calls follow the guard rules
    // minimal other validation in this pass because first pass caught most of it
    for (int i = 8; i < hook.size();)
    {

        int section_type = hook[i++];
        int section_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
        //int section_start = i;
        int next_section = i + section_length;

        if (section_type == 1) // type section
        {
            printf("section_type==1 type section\n");
            int type_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            for (int j = 0; j < type_count; ++j)
            { 
                if (hook[i++] != 0x60)
                {
                    JLOG(ctx.j.trace())
                        << "HookSet(" << hook::log::FUNC_TYPE_INVALID << ")[" 
                        << HS_ACC() << "]: Invalid function type. "
                        << "Codesec: " << section_type << " "
                        << "Local: " << j << " "
                        << "Offset: " << i;
                    return {};
                }
                CHECK_SHORT_HOOK();
                
                int param_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                for (int k = 0; k < param_count; ++k)
                {
                    int param_type = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                    if (param_type == 0x7F || param_type == 0x7E ||
                        param_type == 0x7D || param_type == 0x7C)
                    {
                        // pass, this is fine
                    }
                    else
                    {
                        JLOG(ctx.j.trace())
                            << "HookSet(" << hook::log::FUNC_PARAM_INVALID << ")["
                            << HS_ACC() << "]: Invalid parameter type in function type. "
                            << "Codesec: " << section_type << " "
                            << "Local: " << j << " "
                            << "Offset: " << i;
                        return {};
                    }

                    printf("Function type idx: %d, hook_func_idx: %d, cbak_func_idx: %d "
                           "param_count: %d param_type: %x\n", 
                           j, *hook_func_idx, *cbak_func_idx, param_count, param_type);

                    // hook and cbak parameter check here
                    if ((j == hook_type_idx || (cbak_type_idx && j == cbak_type_idx)) && 
                        (param_count != 1 || param_type != 0x7F /* i32 */ ))
                    {
                        JLOG(ctx.j.trace())
                            << "HookSet(" << hook::log::PARAM_HOOK_CBAK << ")["
                            << HS_ACC() << "]: Malformed transaction. "
                            << "hook and cbak function definition must have exactly one uint32_t parameter.";
                        return {};
                    }
                }

                int result_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                
                // RH TODO: enable this for production
                // this needs a reliable hook cleaner otherwise it will catch most compilers out
                if (0 && result_count != 1)
                {
                    JLOG(ctx.j.trace())
                        << "HookSet(" << hook::log::FUNC_RETURN_COUNT << ")["
                        << HS_ACC() << "]: Malformed transaction. "
                        << "Hook declares a function type that returns fewer or more than one value.";
                    return {};
                }

                // this can only ever be 1 in production, but in testing it may also be 0 or >1
                // so for completeness this loop is here but can be taken out in prod
                for (int k = 0; k < result_count; ++k)
                {
                    int result_type = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                    if (result_type == 0x7F || result_type == 0x7E ||
                        result_type == 0x7D || result_type == 0x7C)
                    {
                        // pass, this is fine
                    }
                    else
                    {
                        JLOG(ctx.j.trace())
                            << "HookSet(" << hook::log::FUNC_RETURN_INVALID << ")["
                            << HS_ACC() << "]: Invalid return type in function type. "
                            << "Codesec: " << section_type << " "
                            << "Local: " << j << " "
                            << "Offset: " << i;
                        return {};
                    }
                    
                    printf("Function type idx: %d, hook_func_idx: %d, cbak_func_idx: %d "
                           "result_count: %d result_type: %x\n", 
                           j, *hook_func_idx, *cbak_func_idx, result_count, result_type);
                        
                    // hook and cbak return type check here
                    if ((j == hook_type_idx || (cbak_type_idx && j == cbak_type_idx)) && 
                        (result_count != 1 || result_type != 0x7E /* i64 */ ))
                    {
                        JLOG(ctx.j.trace())
                            << "HookSet(" << hook::log::RETURN_HOOK_CBAK << ")["
                            << HS_ACC() << "]: Malformed transaction. "
                            << (j == hook_type_idx ? "hook" : "cbak") << " j=" << j << " "
                            << " function definition must have exactly one int64_t return type. "
                            << "resultcount=" << result_count << ", resulttype=" << result_type << ", "
                            << "paramcount=" << param_count;
                        return {};
                    }
                }
            }
        }
        else
        if (section_type == 10) // code section
        {
            // RH TODO: parse anywhere else an expr is allowed in wasm and enforce rules there too
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
                            << "HookSet(" << hook::log::TYPE_INVALID << ")["
                            << HS_ACC() << "]: Invalid local type. "
                            << "Codesec: " << j << " "
                            << "Local: " << k << " "
                            << "Offset: " << i;
                        return {};
                    }
                    i++; CHECK_SHORT_HOOK();
                }

                if (i == code_end)
                    continue; // allow empty functions

                // execution to here means we are up to the actual expr for the codesec/function

                auto valid =
                    check_guard(ctx, hook, j, i, code_end, guard_import_number, last_import_number);

                if (!valid)
                    return {};

                if (hook_func_idx && *hook_func_idx == j)
                    maxInstrCountHook = *valid;
                else if (cbak_func_idx && *cbak_func_idx == j)
                    maxInstrCountCbak = *valid;
                else
                {
                    printf("code section: %d not hook_func_idx: %d or cbak_func_idx: %d\n",
                            j, *hook_func_idx, (cbak_func_idx ? *cbak_func_idx : -1));
                    //   assert(false);
                }

                i = code_end;

            }
        }
        i = next_section;
    }

    // execution to here means guards are installed correctly

    JLOG(ctx.j.trace())
        << "HookSet(" 
        << hook::log::WASM_SMOKE_TEST 
        << ")[" << HS_ACC() << "]: Trying to wasm instantiate proposed hook "
        << "size = " <<  hook.size();

    std::optional<std::string> result = 
        hook::HookExecutor::validateWasm(hook.data(), (size_t)hook.size());

    if (result)
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::WASM_TEST_FAILURE << ")[" << HS_ACC() << "]: "
            << "Tried to set a hook with invalid code. VM error: " << *result;
        return {};
    }

    return std::pair<uint64_t, uint64_t>{maxInstrCountHook, maxInstrCountCbak};
}

// This is a context-free validation, it does not take into account the current state of the ledger
// returns  < valid, instruction count >
// may throw overflow_error
std::variant<
    bool,           // true = valid
    std::pair<      // if set implicitly valid, and return instruction counts (hsoCREATE only)
        uint64_t,   // max instruction count for hook
        uint64_t    // max instruction count for cbak
    >
>
validateHookSetEntry(SetHookCtx& ctx, STObject const& hookSetObj)
{
    uint32_t flags = hookSetObj.isFieldPresent(sfFlags) ? hookSetObj.getFieldU32(sfFlags) : 0;


    switch (inferOperation(hookSetObj))
    {
        case hsoNOOP:
        {
            return true;
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
                return false;
            }

            if (flags != hsfNSDELETE)
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::NSDELETE_FLAGS << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook nsdelete operation should only specify hsfNSDELETE";
                return false;
            }

            return true;
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
                return false;
            }

            if (!(flags & hsfOVERRIDE))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::OVERRIDE_MISSING << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook delete operation was missing the hsfOVERRIDE flag";
                return false;
            }


            if (flags & ~(hsfOVERRIDE | hsfNSDELETE))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::FLAGS_INVALID << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook delete operation specified invalid flags";
                return false;
            }

            return true;
        }

        case hsoINSTALL:
        {
            // validate hook params structure, if any
            if (hookSetObj.isFieldPresent(sfHookParameters) &&
                !validateHookParams(ctx, hookSetObj.getFieldArray(sfHookParameters)))
                return false;

            // validate hook grants structure, if any
            if (hookSetObj.isFieldPresent(sfHookGrants) &&
                !validateHookGrants(ctx, hookSetObj.getFieldArray(sfHookGrants)))
                return false;
            
            // api version not allowed in update
            if (hookSetObj.isFieldPresent(sfHookApiVersion))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::API_ILLEGAL << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook install operation sfHookApiVersion must not be included.";
                return false;
            }
    
            // namespace may be valid, if the user so chooses
            // hookon may be present if the user so chooses
            // flags may be present if the user so chooses

            return true;
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
                return false;
            }

            // validate hook params structure
            if (hookSetObj.isFieldPresent(sfHookParameters) &&
                !validateHookParams(ctx, hookSetObj.getFieldArray(sfHookParameters)))
                return false;

            // validate hook grants structure
            if (hookSetObj.isFieldPresent(sfHookGrants) &&
                !validateHookGrants(ctx, hookSetObj.getFieldArray(sfHookGrants)))
                return false;
            
            // api version not allowed in update
            if (hookSetObj.isFieldPresent(sfHookApiVersion))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::API_ILLEGAL << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook update operation sfHookApiVersion must not be included.";
                return false;
            }

            // namespace may be valid, if the user so chooses
            // hookon may be present if the user so chooses
            // flags may be present if the user so chooses

            return true;
        }

        case hsoCREATE:
        {
            // validate hook params structure
            if (hookSetObj.isFieldPresent(sfHookParameters) &&
                !validateHookParams(ctx, hookSetObj.getFieldArray(sfHookParameters)))
                return false;

            // validate hook grants structure
            if (hookSetObj.isFieldPresent(sfHookGrants) &&
                !validateHookGrants(ctx, hookSetObj.getFieldArray(sfHookGrants)))
                return false;


            // ensure hooknamespace is present
            if (!hookSetObj.isFieldPresent(sfHookNamespace))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::NAMESPACE_MISSING << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHookDefinition must contain sfHookNamespace.";
                return false;
            }

            // validate api version, if provided
            if (!hookSetObj.isFieldPresent(sfHookApiVersion))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::API_MISSING << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHookApiVersion must be included.";
                return false;
            }
                
            auto version = hookSetObj.getFieldU16(sfHookApiVersion);
            if (version != 0)
            {
                // we currently only accept api version 0
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::API_INVALID << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHook->sfHookApiVersion invalid. (Try 0).";
                return false;
            }

            // validate sfHookOn
            if (!hookSetObj.isFieldPresent(sfHookOn))
            {
                JLOG(ctx.j.trace())
                    << "HookSet(" << hook::log::HOOKON_MISSING << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook must include sfHookOn when creating a new hook.";
                return false;
            }
            
            // finally validate web assembly byte code
            {
                auto result = validateCreateCode(ctx, hookSetObj);
                if (!result)
                    return false;
                return *result;
            }
        }
        
        case hsoINVALID:
        default:
        {
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::HASH_OR_CODE << ")[" << HS_ACC()
                << "]: Malformed transaction: SetHook must provide only one of sfCreateCode or sfHookHash.";
            return false;
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
                    << "HookSet(" << hook::log::HOOK_DEF_MISSING << ")[" << HS_ACC()
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
        JLOG(ctx.j.warn())
            << "HookSet(" << hook::log::AMENDMENT_DISABLED << ")["
            << HS_ACC() << "]: Hooks Amendment not enabled!";
        return temDISABLED;
    }

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    if (!ctx.tx.isFieldPresent(sfHooks))
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::HOOKS_ARRAY_MISSING << ")["
            << HS_ACC() << "]: Malformed transaction: SetHook lacked sfHooks array.";
        return temMALFORMED;
    }

    auto const& hookSets = ctx.tx.getFieldArray(sfHooks);

    if (hookSets.size() < 1)
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::HOOKS_ARRAY_EMPTY << ")[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHooks empty.";
        return temMALFORMED;
    }

    if (hookSets.size() > hook::maxHookChainLength())
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::HOOKS_ARRAY_TOO_BIG << ")[" << HS_ACC()
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
                << "HookSet(" << hook::log::HOOKS_ARRAY_BAD << ")[" 
                << HS_ACC()
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
                    << "HookSet(" << hook::log::HOOK_INVALID_FIELD << ")[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHook contains invalid field.";
                return temMALFORMED;
            }
        }

        try
        {

            // may throw if leb128 overflow is detected
            auto valid =
                validateHookSetEntry(shCtx, *hookSetObj);

            if (std::holds_alternative<bool>(valid) && !std::get<bool>(valid))
                return temMALFORMED;

        }
        catch (std::exception& e)
        {
            JLOG(ctx.j.trace())
                << "HookSet(" << hook::log::WASM_VALIDATION
                << ")[" << HS_ACC() << "]: Exception: " << e.what();
            return temMALFORMED;
        }
    }

    if (allBlank)
    {
        JLOG(ctx.j.trace())
            << "HookSet(" << hook::log::HOOKS_ARRAY_BLANK << ")["
            << HS_ACC()
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
    uint256 ns
) {
    JLOG(ctx.j.trace())
        << "HookSet(" << hook::log::NSDELETE << ")[" << HS_ACC() << "]: DeleteState "
        << "Destroying Hook Namespace for " << account << " namespace " << ns;

    Keylet dirKeylet = keylet::hookStateDir(account, ns);

    std::shared_ptr<SLE const> sleDirNode{};
    unsigned int uDirEntry{0};
    uint256 dirEntry{beast::zero};

    auto sleDir = view.peek(dirKeylet);
    
    if (!sleDir || dirIsEmpty(view, dirKeylet))
        return tesSUCCESS;

    auto sleAccount = view.peek(keylet::account(account));
    if (!sleAccount)
    {
        JLOG(ctx.j.fatal())
            << "HookSet(" << hook::log::NSDELETE_ACCOUNT
            << ")[" << HS_ACC() << "]: Account does not exist to destroy namespace from";
        return tefBAD_LEDGER;
    }


    if (!cdirFirst(
            view,
            dirKeylet.key,
            sleDirNode,
            uDirEntry,
            dirEntry)) {
            JLOG(ctx.j.fatal())
                << "HookSet(" << hook::log::NSDELETE_DIRECTORY << ")[" << HS_ACC() << "]: DeleteState "
                << "directory missing ";
        return tefINTERNAL;
    }

    uint32_t stateCount =sleAccount->getFieldU32(sfHookStateCount);
    uint32_t oldStateCount = stateCount;

    std::vector<uint256> toDelete {sleDir->getFieldV256(sfIndexes).size()};
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
                << "HookSet(" << hook::log::NSDELETE_DIR_ENTRY << ")[" << HS_ACC() << "]: DeleteState "
                << "directory node in ledger " << view.seq() << " "
                << "has index to object that is missing: "
                << to_string(dirEntry);
            return tefBAD_LEDGER;
        }

        auto nodeType = sleItem->getFieldU16(sfLedgerEntryType);

        if (nodeType != ltHOOK_STATE)
        {
            JLOG(ctx.j.fatal())
                << "HookSet(" << hook::log::NSDELETE_NONSTATE << ")[" << HS_ACC() << "]: DeleteState "
                << "directory node in ledger " << view.seq() << " "
                << "has non-ltHOOK_STATE entry " << to_string(dirEntry);
            return tefBAD_LEDGER;
        }


        toDelete.push_back(uint256::fromVoid(itemKeylet.key.data()));

    } while (cdirNext(view, dirKeylet.key, sleDirNode, uDirEntry, dirEntry));

    // delete it!
    for (auto const& itemKey: toDelete)
    {

        auto const& sleItem = view.peek({ltHOOK_STATE, itemKey});
    
        if (!sleItem)
        {
            JLOG(ctx.j.warn())
                << "HookSet(" << hook::log::NSDELETE_ENTRY
                << ")[" << HS_ACC() << "]: DeleteState "
                << "Namespace ltHOOK_STATE entry was not found in ledger: "
                << itemKey;
            continue;
        }

        auto const hint = (*sleItem)[sfOwnerNode];
        if (!view.dirRemove(dirKeylet, hint, itemKey, false))
        {
            JLOG(ctx.j.fatal())
                << "HookSet(" << hook::log::NSDELETE_DIR
                << ")[" << HS_ACC() << "]: DeleteState "
                << "directory node in ledger " << view.seq() << " "
                << "could not be deleted.";
            return tefBAD_LEDGER;
        }
        view.erase(sleItem);
        stateCount--;
    }

    if (stateCount > oldStateCount)
    {
        JLOG(ctx.j.fatal())
            << "HookSet(" << hook::log::NSDELETE_COUNT << ")[" << HS_ACC() << "]: DeleteState "
            << "stateCount less than zero (overflow)";

        return tefBAD_LEDGER;
    }

    sleAccount->setFieldU32(sfHookStateCount, stateCount);

    STVector256 const& vec = sleAccount->getFieldV256(sfHookNamespaces);
    if (vec.size() - 1 == 0)
    {
        sleAccount->makeFieldAbsent(sfHookNamespaces);
    }
    else
    {
        std::vector<uint256> nv { vec.size() - 1 };
    
        for (uint256 u : vec.value())
            if (u != ns)
                nv.push_back(u);

        sleAccount->setFieldV256(sfHookNamespaces, STVector256 { std::move(nv) } );
    }

    view.update(sleAccount);

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
            << "HookSet(" << hook::log::HOOK_PARAMS_COUNT << ")[" << HS_ACC()
            << "]: Malformed transaction: Txn would result in too many parameters on hook";
        return tecINTERNAL;
    }

    STArray newParameters {sfHookParameters, parameterCount};
    for (const auto& [parameterName, parameterValue] : parameters)
    {
        if (parameterName.size() > paramKeyMax || parameterValue.size() > paramValueMax)
        {
            JLOG(ctx.j.fatal())
                << "HookSet(" << hook::log::HOOK_PARAM_SIZE << ")[" << HS_ACC()
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

    auto accountSLE = view().peek(accountKeylet);

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
    std::set<uint256> namespacesToDestroy {};

    int hookSetNumber = -1;
    auto const& hookSets = ctx.tx.getFieldArray(sfHooks);

    int hookSetCount = hookSets.size();

    for (hookSetNumber = 0; hookSetNumber < std::max(oldHookCount, hookSetCount); ++hookSetNumber)
    {

        ripple::STObject                                                newHook         { sfHook };
        std::optional<std::reference_wrapper<ripple::STObject const>>   oldHook;
        // an existing hook would only be present if the array slot also exists on the ltHOOK object
        if (hookSetNumber < oldHookCount)
            oldHook = std::cref((oldHooks->get()[hookSetNumber]).downcast<ripple::STObject const>());

        std::optional<std::reference_wrapper<ripple::STObject const>>   hookSetObj;
        if (hookSetNumber < hookSetCount)
            hookSetObj = std::cref((hookSets[hookSetNumber]).downcast<ripple::STObject const>());

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

        uint32_t flags = 0;
        
        if (hookSetObj && hookSetObj->get().isFieldPresent(sfFlags))
            flags = hookSetObj->get().getFieldU32(sfFlags);


        HookSetOperation op = hsoNOOP;
        
        if (hookSetObj)
            op = inferOperation(hookSetObj->get());

        printf("HookSet operation %d: %s\n", hookSetNumber, 
                (op == hsoNSDELETE ? "hsoNSDELETE" :
                (op == hsoDELETE ? "hsoDELETE" :
                (op == hsoCREATE ? "hsoCREATE" :
                (op == hsoINSTALL ? "hsoINSTALL" :
                (op == hsoUPDATE ? "hsoUPDATE" :
                (op == hsoNOOP ? "hsoNOOP" : "hsoINALID")))))));

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
        if (hookSetObj)
        {
            if (hookSetObj->get().isFieldPresent(sfHookHash))
            {
                newDefKeylet = keylet::hookDefinition(hookSetObj->get().getFieldH256(sfHookHash));
                newDefSLE = view().peek(*newDefKeylet);
            }

            if (hookSetObj->get().isFieldPresent(sfHookOn))
                newHookOn = hookSetObj->get().getFieldU64(sfHookOn);

            if (hookSetObj->get().isFieldPresent(sfHookNamespace))
            {
                newNamespace = hookSetObj->get().getFieldH256(sfHookNamespace);
                newDirKeylet = keylet::hookStateDir(account_, *newNamespace);
            }
        }

        // users may destroy a namespace in any operation except NOOP and INVALID
        if (flags & hsfNSDELETE)
        {
            if (op == hsoNOOP || op == hsoINVALID)
            {
                // don't do any namespace deletion
            }
            else if(op == hsoNSDELETE && newDirKeylet)
            {
                printf("Marking a namespace for destruction.... NSDELETE\n");
                namespacesToDestroy.emplace(*newNamespace);
            }
            else if (oldDirKeylet)
            {
                printf("Marking a namespace for destruction.... non-NSDELETE\n");
                namespacesToDestroy.emplace(*oldNamespace);
            }
            else
            {
                JLOG(ctx.j.warn())
                    << "HookSet(" << hook::log::NSDELETE_NOTHING << ")[" << HS_ACC()
                    << "]: SetHook hsoNSDELETE specified but nothing to delete";
            }
        }


        // if there is only an existing hook, without a HookSetObj then it is
        // logically impossible for the operation to not be NOOP
        assert(hookSetObj || op == hsoNOOP);

        switch (op)
        {
            
            case hsoNOOP:
            {
                // if a hook already exists here then migrate it to the new array
                // if it doesn't exist just place a blank object here
                newHooks.push_back( oldHook ? oldHook->get() : ripple::STObject{sfHook} );
                continue;
            }
           
            // every case below here is guarenteed to have a populated hookSetObj
            // by the assert statement above

            case hsoNSDELETE:
            {
                // this case is handled directly above already
                continue;
            }
            
            case hsoDELETE:
            {

                if (!(flags & hsfOVERRIDE))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet(" << hook::log::DELETE_FLAG << ")[" << HS_ACC()
                        << "]: SetHook delete operation requires hsfOVERRIDE flag";
                    return tecREQUIRES_FLAG;
                }
               
                // place an empty corresponding Hook 
                newHooks.push_back(ripple::STObject{sfHook});

                if (!oldHook)
                {
                    JLOG(ctx.j.trace())
                        << "HookSet(" << hook::log::DELETE_NOTHING << ")[" << HS_ACC()
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
                    updateHookParameters(ctx, hookSetObj->get(), oldDefSLE, newHook);

                if (result != tesSUCCESS)
                    return result;

                // if grants are provided set them
                if (hookSetObj->get().isFieldPresent(sfHookGrants))
                    newHook.setFieldArray(sfHookGrants, hookSetObj->get().getFieldArray(sfHookGrants));

                newHooks.push_back(std::move(newHook));
                continue;
            }


            case hsoCREATE:
            {
                if (oldHook && oldHook->get().isFieldPresent(sfHookHash) && !(flags & hsfOVERRIDE))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet(" << hook::log::CREATE_FLAG << ")[" << HS_ACC()
                        << "]: SetHook create operation would override but hsfOVERRIDE flag wasn't specified";
                    return tecREQUIRES_FLAG;
                }
                

                ripple::Blob wasmBytes = hookSetObj->get().getFieldVL(sfCreateCode);

                if (wasmBytes.size() > blobMax)
                {
                    JLOG(ctx.j.warn())
                        << "HookSet(" << hook::log::WASM_TOO_BIG << ")[" << HS_ACC()
                        << "]: Malformed transaction: SetHook operation would create blob larger than max";
                    return tecINTERNAL;
                }

                createHookHash = ripple::sha512Half_s(
                    ripple::Slice(wasmBytes.data(), wasmBytes.size())
                );

                auto keylet = ripple::keylet::hookDefinition(*createHookHash);


                if (view().exists(keylet))
                {
                    newDefSLE = view().peek(keylet);
                    newDefKeylet = keylet;
        
                    // this falls through to hsoINSTALL
                }
                else
                {
                    uint64_t maxInstrCountHook = 0;
                    uint64_t maxInstrCountCbak = 0;
                    bool valid = false;

                    // create hook definition SLE
                    try
                    {

                        auto valid =
                            validateHookSetEntry(ctx, hookSetObj->get());

                        // if invalid return an error
                        if (std::holds_alternative<bool>(valid))
                        {
                            if (!std::get<bool>(valid))
                            {
                                JLOG(ctx.j.warn())
                                    << "HookSet(" << hook::log::WASM_INVALID << ")[" << HS_ACC()
                                    << "]: Malformed transaction: SetHook operation would create invalid hook wasm";
                                return tecINTERNAL;
                            }
                            else
                                assert(false); // should never happen
                        }

                        // otherwise assign instruction counts
                        std::tie(maxInstrCountHook, maxInstrCountCbak) =
                            std::get<std::pair<uint64_t, uint64_t>>(valid);
                    }
                    catch (std::exception& e)
                    {
                        JLOG(ctx.j.warn())
                            << "HookSet(" << hook::log::WASM_INVALID << ")[" << HS_ACC()
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
                            hookSetObj->get().isFieldPresent(sfHookParameters)
                            ? hookSetObj->get().getFieldArray(sfHookParameters)
                            : STArray {} );
                    newHookDef->setFieldU16(    sfHookApiVersion, 
                            hookSetObj->get().getFieldU16(sfHookApiVersion));
                    newHookDef->setFieldVL(     sfCreateCode, wasmBytes);
                    newHookDef->setFieldH256(   sfHookSetTxnID, ctx.tx.getTransactionID());
                    newHookDef->setFieldU64(    sfReferenceCount, 1);
                    newHookDef->setFieldAmount(sfFee,  
                            XRPAmount {hook::computeExecutionFee(maxInstrCountHook)});
                    if (maxInstrCountCbak > 0)
                    newHookDef->setFieldAmount(sfHookCallbackFee,
                            XRPAmount {hook::computeExecutionFee(maxInstrCountCbak)});
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
                        << "HookSet(" << hook::log::INSTALL_FLAG << ")[" << HS_ACC()
                        << "]: SetHook install operation would override but hsfOVERRIDE flag wasn't specified";
                    return tecREQUIRES_FLAG;
                }

                // check if the target hook exists
                if (!newDefSLE)
                {
                    JLOG(ctx.j.trace())
                        << "HookSet(" << hook::log::INSTALL_MISSING << ")[" << HS_ACC()
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
                    createHookHash = hookSetObj->get().getFieldH256(sfHookHash);

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
                    updateHookParameters(ctx, hookSetObj->get(), newDefSLE, newHook);

                if (result != tesSUCCESS)
                    return result;

                // if grants are provided set them
                if (hookSetObj->get().isFieldPresent(sfHookGrants))
                    newHook.setFieldArray(sfHookGrants, hookSetObj->get().getFieldArray(sfHookGrants));

                newHooks.push_back(std::move(newHook));

                view().update(newDefSLE);

                continue;
            }

            case hsoINVALID:
            default:
            {
                JLOG(ctx.j.warn())
                    << "HookSet(" << hook::log::OPERATION_INVALID << ")[" << HS_ACC()
                    << "]: Malformed transaction: sethook could not understand the desired operation.";
                return tecCLAIM;
            }
        }
    }

    {
        // clean up any Namespace directories marked for deletion and any zero reference Hook Definitions
        for (auto const& ns : namespacesToDestroy)
            destroyNamespace(ctx, view(), account_, ns);


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

        // check if the new hook object is empty
        bool newHooksEmpty = true;
        for (auto const& h: newHooks)
        {
            if (h.isFieldPresent(sfHookHash))
            {
                newHooksEmpty = false;
                break;
            }
        }
        

        newHookSLE->setFieldArray(sfHooks, newHooks);
        newHookSLE->setAccountID(sfAccount, account_);

        // There are three possible final outcomes
        // Either the account's ltHOOK is deleted, updated or created.

        if (oldHookSLE && newHooksEmpty)
        {
            // DELETE ltHOOK
            auto const hint = (*oldHookSLE)[sfOwnerNode];
            if (!view().dirRemove(
                        keylet::ownerDir(account_),
                        hint, hookKeylet.key, false))
            {
                JLOG(j_.fatal())
                    << "HookSet(" << hook::log::HOOK_DELETE << ")[" << HS_ACC()
                    << "]: Unable to delete ltHOOK from owner";
                return tefBAD_LEDGER;
            }

            view().erase(oldHookSLE);
    
            // update owner count to reflect removal
            adjustOwnerCount(view(), accountSLE, -1, j_);
            view().update(accountSLE);
        }
        else if (oldHookSLE && !newHooksEmpty)
        {
            // UPDATE ltHOOK
            view().erase(oldHookSLE);
            view().insert(newHookSLE);
        }
        else if (!oldHookSLE && !newHooksEmpty)
        {       
            // CREATE ltHOOK
            auto const page = view().dirInsert(
                keylet::ownerDir(account_),
                hookKeylet,
                describeOwnerDir(account_));
            
            JLOG(j_.trace())
                << "HookSet(" << hook::log::HOOK_ADD << ")[" << HS_ACC()
                << "]: Adding ltHook to account directory "
                << to_string(hookKeylet.key) << ": "
                << (page ? "success" : "failure");

            if (!page)
                return tecDIR_FULL;

            newHookSLE->setFieldU64(sfOwnerNode, *page);
            
            view().insert(newHookSLE);
            
            // update owner count to reflect new ltHOOK object
            adjustOwnerCount(view(), accountSLE, 1, j_);
            view().update(accountSLE);
        }
        else
        {
            // for clarity if this is a NO-OP
        }
    }
    return tesSUCCESS;
}


}  // namespace ripple
