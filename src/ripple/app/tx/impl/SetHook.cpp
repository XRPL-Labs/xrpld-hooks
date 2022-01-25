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

#define DEBUG_GUARD_CHECK 0
#define HS_ACC() ctx.tx.getAccountID(sfAccount) << "-" << ctx.tx.getTransactionID()

namespace ripple {

// RH TODO deal with overflow on leb128
// web assembly contains a lot of run length encoding in LEB128 format
inline uint64_t
parseLeb128(std::vector<unsigned char>& buf, int start_offset, int* end_offset)
{
    uint64_t val = 0, shift = 0, i = start_offset;
    while (i < buf.size())
    {
        int b = (int)(buf[i]);
        val += (b & 0x7F) << shift;
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
           << "HookSet[" << HS_ACC() << "]: Malformed transaction: Hook truncated or otherwise invalid\n";\
        return {false, 0};\
    }\
}

// checks the WASM binary for the appropriate required _g guard calls and rejects it if they are not found
// start_offset is where the codesection or expr under analysis begins and end_offset is where it ends
// returns {valid, worst case instruction count}
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
                    << "HookSet[" << HS_ACC() << "]: GuardCheck "
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
                            << "HookSet[" << HS_ACC() << "]: GuardCheck "
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
                        JLOG(ctx.j.trace()) << "HookSet[" << HS_ACC() << "]: GuardCheck "
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
                                << "HookSet[" << HS_ACC() << "]: GuardCheck "
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
            JLOG(ctx.j.trace()) << "HookSet[" << HS_ACC() << "]: GuardCheck "
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
                JLOG(ctx.j.trace()) << "HookSet[" << HS_ACC() << "]: GuardCheck "
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
                    << "HookSet[" << HS_ACC() << "]: GuardCheck "
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
                    << "HookSet[" << HS_ACC() << "]: GuardCheck "
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
        << "HookSet[" << HS_ACC() << "]: GuardCheck "
        << "Total worse-case execution count: " << instruction_count[0].second;

    // RH TODO: don't hardcode this
    if (instruction_count[0].second > 0xFFFFF)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC() << "]: GuardCheck "
            << "Maximum possible instructions exceed 1048575, please make your hook smaller "
            << "or check your guards!";
        return {false, 0};
    }

    // if we reach the end of the code looking for another trigger the guards are installed correctly
    if (mode == 1)
        return {true, instruction_count[0].second};

    JLOG(ctx.j.trace())
        << "HookSet[" << HS_ACC() << "]: GuardCheck "
        << "Guard did not occur before end of loop / function. "
        << "Codesec: " << codesec;
    return {false, 0};

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
                << "HookSet[" << HS_ACC()
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
                    << "HookSet[" << HS_ACC()
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
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: "
                << "SetHook sfHookParameter must contain at least sfHookParameterName";
            return false;
        }
    }

    return true;
}

// returns  < valid, instruction count >
std::pair<bool, uint64_t>
validateHookSetEntry(SetHookCtx& ctx, STObject const& hookSetObj)
{
    uint64_t maxInstrCount = 0;
    uint64_t byteCount = 0;

    bool hasHash = hookSetObj.isFieldPresent(sfHookHash);
    bool hasCode = hookSetObj.isFieldPresent(sfCreateCode);

    // mutex options: either link an existing hook or create a new one
    if (hasHash && hasCode)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook must provide only one of sfCreateCode or sfHookHash.";
        return {false, 0};
    }

    // validate hook params structure
    if (hookSetObj.isFieldPresent(sfHookParameters) &&
        !validateHookParams(ctx, hookSetObj.getFieldArray(sfHookParameters)))
        return {false, 0};

    // validate hook grants structure
    if (hookSetObj.isFieldPresent(sfHookGrants))
    {
        auto const& hookGrants = hookSetObj.getFieldArray(sfHookGrants);

        if (hookGrants.size() < 1)
        {
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: SetHook sfHookGrants empty.";
            return {false, 0};
        }

        if (hookGrants.size() > 8)
        {
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: SetHook sfHookGrants contains more than 8 entries.";
            return {false, 0};
        }

        for (auto const& hookGrant : hookGrants)
        {
            auto const& hookGrantObj = dynamic_cast<STObject const*>(&hookGrant);
            if (!hookGrantObj || (hookGrantObj->getFName() != sfHookGrant))
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHookGrants did not contain sfHookGrant object.";
                return {false, 0};
            }
            else if (!hookGrantObj->isFieldPresent(sfAuthorize) && !hookGrantObj->isFieldPresent(sfHookHash))
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHookGrant object did not contain either sfAuthorize "
                    << "or sfHookHash.";
                return {false, 0};
            }
        }
    }

    // link existing hook
    if (hasHash)
    {
        // ensure no hookapiversion field was provided
        if (hookSetObj.isFieldPresent(sfHookApiVersion))
        {
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: HookApiVersion can only be provided when creating a new hook.";
            return {false, 0};
        }

        return {true, 0};
    }

    // execution to here means this is an sfCreateCode (hook creation) entry

    // ensure hooknamespace is present
    if (!hookSetObj.isFieldPresent(sfHookNamespace))
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHookDefinition must contain sfHookNamespace.";
        return {false, 0};
    }

    // validate api version, if provided
    if (!hookSetObj.isFieldPresent(sfHookApiVersion))
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHookApiVersion must be included.";
        return {false, 0};
    }
    else
    {
        auto version = hookSetObj.getFieldU16(sfHookApiVersion);
        if (version != 0)
        {
            // we currently only accept api version 0
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: SetHook sfHookDefinition->sfHookApiVersion invalid. (Try 0).";
            return {false, 0};
        }
    }

    // validate sfHookOn
    if (!hookSetObj.isFieldPresent(sfHookOn))
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook must include sfHookOn when creating a new hook.";
        return {false, 0};
    }


    // validate createcode
    Blob hook = hookSetObj.getFieldVL(sfCreateCode);
    if (hook.empty())
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHookDefinition must contain non-blank sfCreateCode.";
        return {false, 0};
    }

    byteCount = hook.size();

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

        // validate the "create code" part if it's present
        [[maybe_unused]]
        auto [valid,  _] =
            validateHookSetEntry(shCtx, *hookSetObj);

        if (!valid)
                return temMALFORMED;
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

    if (!cdirFirst(
            view,
            dirKeylet.key,
            sleDirNode,
            uDirEntry,
            dirEntry)) {
            JLOG(ctx.j.fatal())
                << "HookSet[" << HS_ACC() << "]: DeleteState "
                << "account directory missing " << account;
        return tefINTERNAL;
    }

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
        }


    } while (cdirNext(view, dirKeylet.key, sleDirNode, uDirEntry, dirEntry));

    return tesSUCCESS;
}


#define DIRECTORY_DEC()\
{\
    if (!oldDirSLE)\
    {\
        JLOG(ctx.j.warn())\
            << "HookSet could not find old hook state dir "\
            << HS_ACC() << "!!!";\
        return tecINTERNAL;\
    }\
    uint64_t refCount = oldDirSLE->getFieldU64(sfReferenceCount);\
    printf("refcount1: %d\n", refCount);\
    if (refCount == 0)\
    {\
        JLOG(ctx.j.warn())\
            << "HookSet dir reference count below 0 "\
            << HS_ACC() << "!!!";\
        return tecINTERNAL;\
    }\
    --refCount;\
    printf("refcount2: %d\n", refCount);\
    oldDirSLE->setFieldU64(sfReferenceCount, refCount);\
    view().update(oldDirSLE);\
    if (refCount <= 0)\
        dirsToDestroy[oldDirKeylet->key] =  flags & FLAG_NSDELETE;\
}

#define DIRECTORY_INC()\
{\
    if (!newDirSLE)\
    {\
        newDirSLE = std::make_shared<SLE>(*newDirKeylet);\
        auto const page = view().dirInsert(\
            ownerDirKeylet,\
            newDirKeylet->key,\
            describeOwnerDir(account_));\
        JLOG(ctx.j.trace()) << "Create state dir for account " << toBase58(account_)\
                         << ": " << (page ? "success" : "failure");\
        if (!page)\
            return tecDIR_FULL;\
        newDirSLE->setFieldU64(sfOwnerNode, *page);\
        newDirSLE->setFieldU64(sfReferenceCount, 1);\
        view().insert(newDirSLE);\
    }\
    else\
    {\
        newDirSLE->setFieldU64(sfReferenceCount, newDirSLE->getFieldU64(sfReferenceCount) + 1);\
        view().update(newDirSLE);\
    }\
}

#define DEFINITION_DEC()\
{\
    if (!oldDefSLE)\
    {\
        JLOG(ctx.j.warn())\
            << "HookSet could not find old hook "\
            << HS_ACC() << "!!!";\
        return tecINTERNAL;\
    }\
    uint64_t refCount = oldDefSLE->getFieldU64(sfReferenceCount);\
    if (refCount == 0)\
    {\
        JLOG(ctx.j.warn())\
            << "HookSet def reference count below 0 "\
            << HS_ACC() << "!!!";\
        return tecINTERNAL;\
    }\
    oldDefSLE->setFieldU64(sfReferenceCount, refCount-1);\
    view().update(oldDefSLE);\
    if (refCount <= 0)\
        defsToDestroy[oldDefKeylet->key] = flags & FLAG_OVERRIDE;\
}

#define DEFINITION_INC()\
{\
    newDefSLE->setFieldU64(sfReferenceCount, newDefSLE->getFieldU64(sfReferenceCount) + 1);\
        view().update(newDefSLE);\
}

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
    const int  paramKeyMax = hook::maxHookParameterKeySize();
    const int  paramValueMax = hook::maxHookParameterValueSize();
    auto const accountKeylet = keylet::account(account_);
    auto const ownerDirKeylet = keylet::ownerDir(account_);
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

    std::map<ripple::uint256, bool> defsToDestroy {}; // keylet => override was in flags
    std::map<ripple::uint256, bool> dirsToDestroy {}; // keylet => nsdelete was in flags

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
        // blank hookSet entries are allowed and means semantically: "do nothing to this hook entry in the chain"
        if (hookSetObj->getCount() == 0)
        {
            // if a hook already exists here then migrate it to the new array
            // if it doesn't exist just place a blank object here
            newHooks.push_back( oldHook ? oldHook->get() : ripple::STObject{sfHook} );
            continue;
        }

        // execution to here means the hookset entry is not blank

        std::optional<ripple::uint256>                                  oldNamespace;
        std::optional<ripple::uint256>                                  defNamespace;
        std::optional<ripple::Keylet>                                   oldDirKeylet;
        std::optional<ripple::Keylet>                                   oldDefKeylet;
        std::optional<ripple::Keylet>                                   newDefKeylet;
        std::shared_ptr<STLedgerEntry>                                  oldDefSLE;
        std::shared_ptr<STLedgerEntry>                                  newDefSLE;
        std::shared_ptr<STLedgerEntry>                                  oldDirSLE;
        std::shared_ptr<STLedgerEntry>                                  newDirSLE;

        std::optional<ripple::uint256>                                  newNamespace;
        std::optional<ripple::Keylet>                                   newDirKeylet;

        std::optional<uint64_t>                                         oldHookOn;
        std::optional<uint64_t>                                         newHookOn;
        std::optional<uint64_t>                                         defHookOn;

        /**
         * This is the primary HookSet loop. We iterate the sfHooks array inside the txn
         * each entry of this array is available as hookSetObj.
         * Depending on whether or not an existing hook is present in the array slot we are currently up to
         * this hook and its various attributes are available in the optionals prefixed with old.
         * Even if an existing hook is being modified by the sethook obj, we create a newHook obj
         * so a degree of copying is required.
         */


        bool        hasHookHash       = hookSetObj->isFieldPresent(sfHookHash);
        bool        hasCreateCode     = hookSetObj->isFieldPresent(sfCreateCode);
        bool        hasParameters     = hookSetObj->isFieldPresent(sfHookParameters);
        bool        hasGrants         = hookSetObj->isFieldPresent(sfHookGrants);

        uint32_t    flags             = hookSetObj->isFieldPresent(sfFlags) ? hookSetObj->getFieldU32(sfFlags) : 0;

        bool        isDeleteOperation =
                        hasCreateCode && hookSetObj->getFieldVL(sfCreateCode).size() == 0;

        printf("PATH X\n");
        bool        isUpdateOperation =
                        oldHook && hasHookHash &&
                        (hookSetObj->getFieldH256(sfHookHash) == oldHook->get().getFieldH256(sfHookHash));

        bool        isCreateOperation =
                        !isUpdateOperation && !isDeleteOperation && hasCreateCode;

        bool        isInstallOperation =
                        !isUpdateOperation && !isDeleteOperation && hasHookHash;

        /**
         * Variables for logic:
         * hasHookHash          <=> the current HookSet operation contains a sfHookHash (not an sfCreateCode)
         * hasCreateCode        <=> the current HookSet operation contains a sfCreateCode (not sfHookHash)
         * isDeleteOperation    <=> the current HookSet operation contains a blank sfCreateCode
         * isUpdateOperation    <=> old hook exists and the current operation updates it
         * isCreateOperation    <=> old hook does not exist and is not a delete operaiton and code is present
         * isInstallOperation   <=> old hook does not exist, and we're installing from a hash
        */


        printf("PATH Y\n");
        // if an existing hook exists at this position in the chain then extract the relevant fields
        if (oldHook)
        {
            // certain actions require explicit flagging to prevent user error
            if (!(flags & FLAG_OVERRIDE) && !isUpdateOperation)
            {
                // deletes (and creates that override an existing hook) require a flag
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: sethook entry must be flagged for override.";
                return tecREQUIRES_FLAG;
            }
            printf("PATH Z\n");
            oldDefKeylet = keylet::hookDefinition(oldHook->get().getFieldH256(sfHookHash));
            oldDefSLE = view().peek(*oldDefKeylet);
            defNamespace = oldDefSLE->getFieldH256(sfHookNamespace);
            oldNamespace = oldHook->get().isFieldPresent(sfHookNamespace)
                    ? oldHook->get().getFieldH256(sfHookNamespace)
                    : *defNamespace;
            oldDirKeylet = keylet::hookStateDir(account_, *oldNamespace);
            oldDirSLE = view().peek(*oldDirKeylet);
            defHookOn = oldDefSLE->getFieldU64(sfHookOn);
            oldHookOn = oldHook->get().isFieldPresent(sfHookOn)
                    ? oldHook->get().getFieldU64(sfHookOn)
                    : *defHookOn;
        }


        if (hasHookHash)
        {
            newDefKeylet = keylet::hookDefinition(hookSetObj->getFieldH256(sfHookHash));
            newDefSLE = view().peek(*newDefKeylet);
        }

        if (hookSetObj->isFieldPresent(sfHookOn))
            newHookOn = hookSetObj->getFieldU64(sfHookOn);

        // if the sethook txn specifies a new namespace then extract those fields
        if (hookSetObj->isFieldPresent(sfHookNamespace))
        {
            newNamespace = hookSetObj->getFieldH256(sfHookNamespace);
            newDirKeylet = keylet::hookStateDir(account_, *newNamespace);
            newDirSLE = view().peek(*newDirKeylet);
        }

        if (oldDirSLE)
        {
            printf("PATH A\n");
            DIRECTORY_DEC();
        }
        else
            printf("PATH B\n");

        if (oldDefSLE)
        {
            printf("PATH C\n");
            DEFINITION_DEC();
        }
        else
            printf("PATH D\n");

        // handle delete operation
        if (isDeleteOperation)
        {
            newHooks.push_back(ripple::STObject{sfHook});
            continue;
        }

        // if we're not performing a delete operation then we must have a newDirKeylet and newDirSLE
        // otherwise we will not be able to create/update a state directory
        if (!newDirKeylet)
        {
            if (newDefSLE)
                newDirKeylet = keylet::hookStateDir(account_, newDefSLE->getFieldH256(sfHookNamespace));
            else if (oldDirKeylet)
                newDirKeylet = oldDirKeylet;
            else
            {
                JLOG(ctx.j.warn())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: sethook could not find a namespace to place hook state into.";
                return tecINTERNAL;
            }
            newDirSLE = view().peek(*newDirKeylet);
        }

        DIRECTORY_INC();

        // handle create operation
        if (isCreateOperation)
        {
            ripple::Blob wasmBytes = hookSetObj->getFieldVL(sfCreateCode);

            if (wasmBytes.size() > blobMax)
            {
                JLOG(ctx.j.warn())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook operation would create blob larger than max";
                return tecINTERNAL;
            }


            auto hash = ripple::sha512Half_s(
                ripple::Slice(wasmBytes.data(), wasmBytes.size())
            );

            // update hook hash
            newHook.setFieldH256(sfHookHash, hash);

            auto keylet = ripple::keylet::hookDefinition(hash);

            if (view().exists(keylet))
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: SetHook operation would create a duplicate wasm blob, using hash only";

                // update reference count
                newDefSLE = view().peek(keylet);
                newDefKeylet = keylet;
                isInstallOperation = true;
                isCreateOperation = false;

                // this falls through to install
            }
            else
            {
                // create hook definition SLE
                auto [valid, maxInstrCount] =
                    validateHookSetEntry(ctx, *hookSetObj);

                if (!valid)
                {
                    JLOG(ctx.j.warn())
                        << "HookSet[" << HS_ACC()
                        << "]: Malformed transaction: SetHook operation would create invalid hook wasm";
                    return tecINTERNAL;
                }

                auto newHookDef = std::make_shared<SLE>( keylet );
                newHookDef->setFieldH256(sfHookHash, hash);
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
                newHookDef->setFieldAmount(sfFee,  XRPAmount { hook::computeExecutionFee(maxInstrCount) } );
                view().insert(newHookDef);
                newHooks.push_back(std::move(newHook));
                continue;
            }
        }
        else if (isInstallOperation) // this needs to be here to allow duplicate wasm blob fallthrough case above
            newHook.setFieldH256(sfHookHash, hookSetObj->getFieldH256(sfHookHash));



        // install operations are half way between create and update operations
        // here we install an existing hook definition into an unused hook slot
        // but care must be taken to ensure parameters, namespaces etc are as the user intends
        // without needlessly duplicating these from the hook definition
        if (isInstallOperation)
        {
            DEFINITION_INC();

            if (newNamespace && *defNamespace != *newNamespace)
                newHook.setFieldH256(sfHookNamespace, *newNamespace);

            if (newHookOn && defHookOn != *newHookOn)
                newHook.setFieldU64(sfHookOn, *newHookOn);

            std::map<ripple::Blob, ripple::Blob> parameters;

            // first pull the parameters into a map
            auto const& hookParameters = hookSetObj->getFieldArray(sfHookParameters);
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
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: Txn would result in too many parameters on hook";
                return tecINTERNAL;
            }

            STArray newParameters {sfHookParameters, parameterCount};
            for (const auto& [parameterName, parameterValue] : parameters)
            {
                STObject param { sfHookParameter };
                param.setFieldVL(sfHookParameterName, parameterName);
                param.setFieldVL(sfHookParameterValue, parameterValue);
                newParameters.push_back(std::move(param));
            }

            newHook.setFieldArray(sfHookParameters, std::move(newParameters));

            if (hasGrants)
                newHook.setFieldArray( sfHookGrants, hookSetObj->getFieldArray(sfHookGrants));

            newHooks.push_back(std::move(newHook));
            continue;
        }


        // handle update operation
        if (isUpdateOperation)
        {
            DEFINITION_INC();

            newHook.setFieldH256(sfHookHash, hookSetObj->getFieldH256(sfHookHash));

            // handle HookOn update logic
            if (hookSetObj->isFieldPresent(sfHookOn))
            {
                uint64_t newHookOn = hookSetObj->getFieldU64(sfHookOn);
                if (newHookOn != defHookOn)
                    newHook.setFieldU64(sfHookOn, hookSetObj->getFieldU64(sfHookOn));
            }
            else if (*oldHookOn != *defHookOn)
                newHook.setFieldU64(sfHookOn, *oldHookOn);


            // handle namespace update logic
            if (newNamespace && *newNamespace != *defNamespace)
                    newHook.setFieldH256(sfHookNamespace, *newNamespace);
            else if (oldNamespace && *oldNamespace != *defNamespace)
                    newHook.setFieldH256(sfHookNamespace, *oldNamespace);

            // process the parameters
            if (hasParameters)
            {
                fprintf(stderr, "PATH L\n");
                std::map<ripple::Blob, ripple::Blob> parameters;

                // gather up existing parameters, but only if this is an update
                if (oldHook->get().isFieldPresent(sfHookParameters))
                {
                    fprintf(stderr, "PATH M\n");
                    auto const& oldParameters = oldHook->get().getFieldArray(sfHookParameters);
                    for (auto const& hookParameter : oldParameters)
                    {
                        fprintf(stderr, "PATH N\n");
                        auto const& hookParameterObj = dynamic_cast<STObject const*>(&hookParameter);
                        parameters[hookParameterObj->getFieldVL(sfHookParameterName)] =
                            hookParameterObj->getFieldVL(sfHookParameterValue);
                    }
                }

                fprintf(stderr, "PATH O\n");
                // process hookset parameters
                auto const& hookParameters = hookSetObj->getFieldArray(sfHookParameters);
                for (auto const& hookParameter : hookParameters)
                {
                    fprintf(stderr, "PATH P\n");
                    auto const& hookParameterObj = dynamic_cast<STObject const*>(&hookParameter);
                    if (!hookParameterObj->isFieldPresent(sfHookParameterName))
                    {
                        fprintf(stderr, "PATH Q\n");
                        JLOG(ctx.j.trace())
                            << "HookSet[" << HS_ACC()
                            << "]: Malformed transaction: Parameter without ParameterName";
                        return tecINTERNAL;
                    }

                    ripple::Blob paramName = hookParameterObj->getFieldVL(sfHookParameterName);
                    fprintf(stderr, "PATH R\n");
                    if (paramName.size() > paramKeyMax)
                    {
                        fprintf(stderr, "PATH S\n");
                        JLOG(ctx.j.trace())
                            << "HookSet[" << HS_ACC()
                            << "]: Malformed transaction: ParameterName too large";
                        return tecINTERNAL;
                    }

                    fprintf(stderr, "PATH T\n");
                    if (hookParameterObj->isFieldPresent(sfHookParameterValue))
                    {
                        fprintf(stderr, "PATH U\n");
                        // parameter update or set operation
                        ripple::Blob newValue = hookParameterObj->getFieldVL(sfHookParameterValue);
                        fprintf(stderr, "PATH V\n");
                        if (newValue.size() > paramValueMax)
                        {
                            fprintf(stderr, "PATH W\n");
                            JLOG(ctx.j.trace())
                                << "HookSet[" << HS_ACC()
                                << "]: Malformed transaction: ParameterValue too large";
                            return tecINTERNAL;
                        }
                        parameters[hookParameterObj->getFieldVL(sfHookParameterName)] = newValue;
                    }
                    else
                    {

                        fprintf(stderr, "PATH @\n");
                        // parameter delete operation
                        parameters.erase(hookParameterObj->getFieldVL(sfHookParameterName));
                    }
                }

                fprintf(stderr, "PATH #\n");
                // remove any duplicate entries that exist in the sle
                auto const& defParameters = newDefSLE->getFieldArray(sfHookParameters);
                for (auto const& hookParameter : defParameters)
                {
                    fprintf(stderr, "PATH $\n");
                    auto const& hookParameterObj = dynamic_cast<STObject const*>(&hookParameter);
                    ripple::Blob n = hookParameterObj->getFieldVL(sfHookParameterName);
                    ripple::Blob v = hookParameterObj->getFieldVL(sfHookParameterValue);

                    if (parameters.find(n) != parameters.end() && parameters[n] == v)
                        parameters.erase(n);
                }

                fprintf(stderr, "PATH %\n");
                int parameterCount = (int)(parameters.size());
                if (parameterCount > 16)
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC()
                        << "]: Malformed transaction: Txn would result in too many parameters on hook";
                    return tecINTERNAL;
                }

                fprintf(stderr, "PATH ^\n");
                STArray newParameters {sfHookParameters, parameterCount};
                for (const auto& [parameterName, parameterValue] : parameters)
                {
                    fprintf(stderr, "PATH &\n");
                    STObject param { sfHookParameter };
                    param.setFieldVL(sfHookParameterName, parameterName);
                    param.setFieldVL(sfHookParameterValue, parameterValue);
                    newParameters.push_back(std::move(param));
                }

                fprintf(stderr, "PATH *\n");
                newHook.setFieldArray(sfHookParameters, std::move(newParameters));
            }

            // process the grants
            if (hasGrants)
                newHook.setFieldArray(sfHookGrants, hookSetObj->getFieldArray(sfHookGrants));

            fprintf(stderr, "PATH (\n");
            newHooks.push_back(std::move(newHook));
            fprintf(stderr, "PATH )\n");
            continue;
        }


        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: Ambiguous sethook entry";
        return tecINTERNAL;

    }

    // clean up any zero reference dirs and defs

    // dirs
    for (auto const& p : dirsToDestroy)
    {
        auto const& sle = view().peek(ripple::Keylet { ltDIR_NODE, p.first });
        uint64_t refCount = sle->getFieldU64(sfReferenceCount);
        if (refCount <= 0)
        {
            if (!p.second)
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook operation would delete a namespace of hook states"
                    << " but to do this you must set the NSDELETE flag";
                return tecREQUIRES_FLAG;
            }

            destroyNamespace(ctx, view(), account_, { ltDIR_NODE, p.first } );
            view().erase(sle);
        }
    }


    // defs
    for (auto const& p : defsToDestroy)
    {
        auto const& sle = view().peek(ripple::Keylet { ltHOOK_DEFINITION, p.first });
        uint64_t refCount = sle->getFieldU64(sfReferenceCount);
        if (refCount <= 0)
        {
            if (!p.second)
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook operation would delete a hook"
                    << " but to do this you must set the OVERRIDE flag";
                return tecREQUIRES_FLAG;
            }
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
