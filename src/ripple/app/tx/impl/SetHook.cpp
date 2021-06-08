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
#include "common/value.h"
#include "vm/configure.h"
#include "vm/vm.h"
#include "common/errcode.h"
#include "runtime/hostfunc.h"
#include "runtime/importobj.h"

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
        return false;\
    }\
}

// RH TODO find a better home for this or a better solution?
const std::set<std::string> import_whitelist
{
    "accept",
    "emit",
    "etxn_burden",
    "etxn_details",
    "etxn_fee_base",
    "etxn_generation",
    "etxn_reserve",
    "float_compare",
    "float_divide",
    "float_exponent",
    "float_exponent_set",
    "float_invert",
    "float_mantissa",
    "float_mantissa_set",
    "float_mulratio",
    "float_multiply",
    "float_int",
    "float_negate",
    "float_one",
    "float_set",
    "float_sign",
    "float_sign_set",
    "float_sto",
    "float_sto_set",
    "float_sum",
    "fee_base",
    "_g",
    "hook_account",
    "hook_hash",
    "ledger_seq",
    "ledger_last_hash",
    "nonce",
    "otxn_burden",
    "otxn_field",
    "otxn_slot",
    "otxn_generation",
    "otxn_id",
    "otxn_type",
    "rollback",
    "slot",
    "slot_clear",
    "slot_count",
    "slot_id",
    "slot_set",
    "slot_size",
    "slot_subarray",
    "slot_subfield",
    "slot_type",
    "slot_float",
    "state",
    "state_foreign",
    "state_set",
    "trace",
    "trace_num",
    "trace_float",
    "trace_slot",
    "util_accid",
    "util_raddr",
    "util_sha512h",
    "util_verify",
    "sto_subarray",
    "sto_subfield",
    "sto_validate",
    "sto_emplace",
    "sto_erase",
    "util_keylet"
};

#define DEBUG_GUARD_CHECK 0

// checks the WASM binary for the appropriate required _g guard calls and rejects it if they are not found
// start_offset is where the codesection or expr under analysis begins and end_offset is where it ends
bool
check_guard(
        PreflightContext const& ctx, ripple::Blob& hook, int codesec,
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

    // block depth level -> { largest guard, rolling instruction count } //RH UPTO
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
                return false;
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
                        return false;
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
                        return false;
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
            return false;
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
                return false;
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
                return false;
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
                return false;
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
        return false;
    }

    // if we reach the end of the code looking for another trigger the guards are installed correctly
    if (mode == 1)
        return true;

    JLOG(ctx.j.trace())
        << "HookSet[" << HS_ACC() << "]: GuardCheck "
        << "Guard did not occur before end of loop / function. "
        << "Codesec: " << codesec;
    return false;

}

bool
validateHookParams(STArray const& hookParams, PreflightContext const& ctx)
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

bool validateHookSetEntry(STObject const& hookSetObj, PreflightContext const& ctx)
{
    bool hasHash = hookSetObj.isFieldPresent(sfHookHash);
    bool hasCode = hookSetObj.isFieldPresent(sfCreateCode);

    // mutex options: either link an existing hook or create a new one
    if (hasHash && hasCode)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook must provide only one of sfCreateCode or sfHookHash.";
        return false;
    }

    // validate hook params structure
    if (hookSetObj.isFieldPresent(sfHookParameters) &&
        !validateHookParams(hookSetObj.getFieldArray(sfHookParameters), ctx))
        return false;

    // validate hook grants structure
    if (hookSetObj.isFieldPresent(sfHookGrants))
    {
        auto const& hookGrants = hookSetObj.getFieldArray(sfHookGrants);

        if (hookGrants.size() < 1)
        {
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: SetHook sfHookGrants empty.";
            return false;
        }

        if (hookGrants.size() > 8)
        {
            JLOG(ctx.j.trace())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: SetHook sfHookGrants contains more than 8 entries.";
            return false;
        }

        for (auto const& hookGrant : hookGrants)
        {
            auto const& hookGrantObj = dynamic_cast<STObject const*>(&hookGrant);
            if (!hookGrantObj || (hookGrantObj->getFName() != sfHookGrant))
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook sfHookGrants did not contain sfHookGrant object.";
                return false;
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
            return false;
        }

        // RH REVIEW: should preflight be checking this?
        auto const view = ctx.app.openLedger().current();

        // check if the specified hook exists
        auto const& hash = hookSetObj.getFieldH256(sfHookHash);
        {
            Keylet const key{ltHOOK_DEFINITION, hash};
            if (!view->exists(key))
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: No hook exists with the specified hash.";
                return false;
            }
        }

        return true;
    }

    // execution to here means this is an sfCreateCode (hook creation) entry

    // ensure hooknamespace is present
    if (!hookSetObj.isFieldPresent(sfHookNamespace))
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHookDefinition must contain sfHookNamespace.";
        return false;
    }

    // validate api version, if provided
    if (!hookSetObj.isFieldPresent(sfHookApiVersion))
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHookApiVersion must be included.";
        return false;
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
            return false;
        }
    }

    // validate sfHookOn
    if (!hookSetObj.isFieldPresent(sfHookOn))
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook must include sfHookOn when creating a new hook.";
        return false;
    }


    // validate createcode
    Blob hook = hookSetObj.getFieldVL(sfCreateCode);
    if (hook.empty())
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHookDefinition must contain non-blank sfCreateCode.";
        return false;
    }


    // RH TODO compute actual smallest possible hook and update this value
    if (hook.size() < 10)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC() << "]: "
            << "Malformed transaction: Hook was not valid webassembly binary. Too small.";
        return false;
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
            return false;
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
            return false;
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
                return false;
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
                    return false;
                }

                if (std::string_view( (const char*)(hook.data() + i), (size_t)mod_length ) != "env")
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to specify import module other than 'env'";
                    return false;
                }

                i += mod_length; CHECK_SHORT_HOOK();

                // next get import name
                int name_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (name_length < 1 || name_length > (hook.size() - i))
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to specify nil or invalid import name";
                    return false;
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
                } else if (import_whitelist.find(import_name) == import_whitelist.end())
                {
                    JLOG(ctx.j.trace())
                        << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                        << "Hook attempted to import a function that does not "
                        << "appear in the hook_api function set: `" << import_name << "`";
                    return false;
                }
                func_upto++;
            }

            if (guard_import_number == -1)
            {
                JLOG(ctx.j.trace())
                    << "HookSet[" << HS_ACC() << "]: Malformed transaction. "
                    << "Hook did not import _g (guard) function";
                return false;
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
                return false;
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
                return false;
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
                        return false;
                    }
                    i++; CHECK_SHORT_HOOK();
                }

                if (i == code_end)
                    continue; // allow empty functions

                // execution to here means we are up to the actual expr for the codesec/function

                if (!check_guard(ctx, hook, j, i, code_end, guard_import_number, last_import_number))
                    return false;

                i = code_end;

            }
        }
        i = next_section;
    }

    // execution to here means guards are installed correctly

    JLOG(ctx.j.trace())
        << "HookSet[" << HS_ACC() << "]: Trying to wasm instantiate proposed hook "
        << "size = " <<  hook.size();

    // check if wasm can be run
    SSVM::VM::Configure cfg;
    SSVM::VM::VM vm(cfg);
    if (auto res = vm.loadWasm(SSVM::Span<const uint8_t>(hook.data(), hook.size())))
    {
        // do nothing
    } else
    {
        uint32_t ssvm_error = static_cast<uint32_t>(res.error());
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC() << "]: "
            << "Tried to set a hook with invalid code. SSVM error: " << ssvm_error;
        return false;
    }

    return true;
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

    if (hookSets.size() > 8)
    {
        JLOG(ctx.j.trace())
            << "HookSet[" << HS_ACC()
            << "]: Malformed transaction: SetHook sfHooks contains more than 8 entries.";
        return temMALFORMED;
    }

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
        if (!validateHookSetEntry(*hookSetObj, ctx))
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
    Application& app,
    ApplyView& view,
    const AccountID& account,
    const Keylet & dirKeylet        // the keylet of the namespace directory
) {
    auto const& ctx = ctx_;
    auto j = app.journal("View");
    JLOG(j.trace())
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
            dirEntry,
            j)) {
            JLOG(j.fatal())
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
            JLOG(j.fatal())
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
                JLOG(j.fatal())
                    << "HookSet[" << HS_ACC() << "]: DeleteState "
                    << "directory node in ledger " << view.seq() << " "
                    << "has undeletable ltHOOK_STATE";
                return tefBAD_LEDGER;
            }
            view.erase(sleItem);
        }


    } while (cdirNext(
        view, dirKeylet.key, sleDirNode, uDirEntry, dirEntry, j));

    return tesSUCCESS;
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

    auto const& ctx = ctx_;
    auto viewJ = ctx_.app.journal("View");

    const int  blobMax = hook::maxHookWasmSize();
    const int  paramMax = hook::maxHookParameterSize();
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
            newHooks[hookSetNumber] = ( oldHook ? oldHook->get() : ripple::STObject{sfHook} );
            continue;
        }

        // execution to here means the hookset entry is not blank

        std::optional<ripple::uint256>                                  oldNamespace;
        std::optional<ripple::Keylet>                                   oldDirKeylet;
        std::optional<ripple::Keylet>                                   oldDefKeylet;
        std::shared_ptr<STLedgerEntry>                                  oldDefSLE;
        std::shared_ptr<STLedgerEntry>                                  oldDirSLE;

        std::optional<ripple::uint256>                                  newNamespace;
        std::optional<ripple::Keylet>                                   newDirKeylet;
        ripple::STObject                                                newHook         { sfHook };

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
        bool        oldHookExists     = oldHook && oldHook->get().isFieldPresent(sfCreateCode);

        bool        isDeleteOperation =
                        hasCreateCode && hookSetObj->getFieldVL(sfCreateCode).size() == 0;

        bool        isUpdateOperation =
                        oldHookExists && hasHookHash &&
                        (hookSetObj->getFieldH256(sfHookHash) == oldHook->get().getFieldH256(sfHookHash));
        
        bool        isCreateOperation =
                        !isUpdateOperation && !isDeleteOperation && hasCreateCode;


        /**
         * Variables for logic:
         * hasHookHash          <=> the current HookSet operation contains a sfHookHash (not an sfCreateCode)
         * hasCreateCode        <=> the current HookSet operation contains a sfCreateCode (not sfHookHash)
         * isDeleteOperation    <=> the current HookSet operation contains a blank sfCreateCode
         * isUpdateOperation    <=> old hook exists and the current operation updates it
         * isCreateOperation    <=> old hook does not exist and is not a delete operaiton and code is present
         * oldHookExists        <=> old hook exists and is not blank
        */

        // certain actions require explicit flagging to prevent user error
        if (!(flags & FLAG_OVERRIDE) && oldHookExists && !isUpdateOperation)
        {
            // deletes (and creates that override an existing hook) require a flag
            JLOG(viewJ.trace())
                << "HookSet[" << HS_ACC()
                << "]: Malformed transaction: sethook entry must be flagged for override.";
            return tecREQUIRES_FLAG;
        }

        // if an existing hook exists at this position in the chain then extract the relevant fields
        if (oldHookExists)
        {
            oldNamespace = oldHook->get().getFieldH256(sfHookNamespace);
            oldDirKeylet = keylet::hookStateDir(account_, *oldNamespace);
            oldDefKeylet = keylet::hookDefinition(oldHook->get().getFieldH256(sfHookHash));
            oldDirSLE = view().peek(*oldDirKeylet);
            oldDefSLE = view().peek(*oldDefKeylet);
        }

        // if the sethook txn specifies a new namespace then extract those fields
        if (hookSetObj->isFieldPresent(sfHookNamespace))
        {
            newNamespace = hookSetObj->getFieldH256(sfHookNamespace);
            newDirKeylet = keylet::hookStateDir(account_, *newNamespace);
        }

        // update reference counts as appropriate for the operation we are performing
        {
            // decrement reference count of HOOK_STATE_DIR when appropriate
            if ((newNamespace && *oldNamespace != *newNamespace) ||     // namespace override
                    isDeleteOperation)                                  // or deleteop
            {
                if (!oldDirSLE)
                {
                    JLOG(viewJ.warn())
                        << "HookSet could not find old hook state dir"
                        << HS_ACC() << "!!!";
                    return tecINTERNAL;
                }

                // decrement reference count on state directory

                uint64_t refCount = oldDirSLE->getFieldU64(sfReferenceCount) - 1;

                oldDirSLE->setFieldU64(sfReferenceCount, refCount);
                view().update(oldDirSLE);

                if (refCount <= 0)
                    dirsToDestroy[oldDirKeylet->key] =  flags & FLAG_NSDELETE;
            }

            // decrement reference count of ltHOOK_DEFINITIOn when appropriate
            if (isDeleteOperation || (isCreateOperation && oldHookExists))
            {
                if (!oldDefSLE)
                {
                    JLOG(viewJ.warn())
                        << "HookSet could not find old hook "
                        << HS_ACC() << "!!!";
                    return tecINTERNAL;
                }

                // decrement reference count on hook definition

                uint64_t refCount = oldDefSLE->getFieldU64(sfReferenceCount) - 1;

                oldDefSLE->setFieldU64(sfReferenceCount, refCount);
                view().update(oldDefSLE);

                if (refCount <= 0)
                    defsToDestroy[oldDefKeylet->key] = flags & FLAG_OVERRIDE;
            }
        }

        // handle delete operation
        if (isDeleteOperation)
        {
            newHooks[hookSetNumber] = ripple::STObject{sfHook};
            continue;
        }


        // handle create operation
        if (isCreateOperation)
        {
            ripple::Blob wasmBytes = hookSetObj->getFieldVL(sfCreateCode);

            if (wasmBytes.size() > blobMax)
            {
                JLOG(viewJ.warn())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook operation would create blob larger than max";
                return tecINTERNAL;
            }

            auto hash = ripple::sha512Half(
                ripple::HashPrefix::hookDefinition,
                wasmBytes
            );

            auto keylet = ripple::keylet::hookDefinition(hash);

            if (view().exists(keylet))
            {
                JLOG(viewJ.warn())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook operation would create a duplicate wasm blob";
                return tecINTERNAL;
            }

            // update hook hash
            newHook.setFieldH256(sfHookHash, hash);

            // create hook definition SLE
            auto newHookDef = std::make_shared<SLE>( keylet );
            newHookDef->setFieldU64(    sfHookOn, hookSetObj->getFieldU64(sfHookOn));
            newHookDef->setFieldH256(   sfHookNamespace, *newNamespace);
            newHookDef->setFieldArray(  sfHookParameters, hookSetObj->getFieldArray(sfHookParameters));
            newHookDef->setFieldU16(    sfHookApiVersion, hookSetObj->getFieldU16(sfHookApiVersion));
            newHookDef->setFieldVL(     sfCreateCode, wasmBytes);
            newHookDef->setFieldH256(   sfHookSetTxnID, ctx.tx.getTransactionID());
            newHookDef->setFieldU64(    sfReferenceCount, 1);

            // create sfHook entry
            newHook.setFieldU64(       sfHookOn, hookSetObj->getFieldU64(sfHookOn));
            newHook.setFieldH256(      sfHookNamespace, *newNamespace);
            newHook.setFieldU16(       sfHookApiVersion, hookSetObj->getFieldU16(sfHookApiVersion));
            newHook.setFieldH256(      sfHookHash, hash);
            newHook.setFieldH256(      sfHookSetTxnID, ctx.tx.getTransactionID());

            if (hasParameters)
                newHook.setFieldArray( sfHookParameters, hookSetObj->getFieldArray(sfHookParameters));

            if (hasGrants)
                newHook.setFieldArray( sfHookGrants, hookSetObj->getFieldArray(sfHookGrants));

            newHooks[hookSetNumber] = std::move(newHook);
            view().insert(newHookDef);
            continue;
        }


        // handle update operation
        if (isUpdateOperation)
        {
            newHook.setFieldH256(   sfHookHash, hookSetObj->getFieldH256(sfHookHash));
            newHook.setFieldU64(    sfHookOn, hookSetObj->getFieldU64(sfHookOn));
            newHook.setFieldH256(   sfHookNamespace, *newNamespace);
            newHook.setFieldU16(    sfHookApiVersion, hookSetObj->getFieldU16(sfHookApiVersion));

            // process the parameters
            if (hasParameters)
            {
                std::map<ripple::Blob, ripple::Blob> parameters;

                // gather up existing parameters
                if (oldHook->get().isFieldPresent(sfHookParameters))
                {
                    auto const& oldParameters = oldHook->get().getFieldArray(sfHookParameters);
                    for (auto const& hookParameter : oldParameters)
                    {
                        auto const& hookParameterObj = dynamic_cast<STObject const*>(&hookParameter);
                        parameters[hookParameterObj->getFieldVL(sfHookParameterName)] =
                            hookParameterObj->getFieldVL(sfHookParameterValue);
                    }
                }

                // process hookset parameters
                auto const& hookParameters = hookSetObj->getFieldArray(sfHookParameters);
                for (auto const& hookParameter : hookParameters)
                {
                    auto const& hookParameterObj = dynamic_cast<STObject const*>(&hookParameter);
                    if (!hookParameterObj->isFieldPresent(sfHookParameterName))
                    {
                        JLOG(viewJ.trace())
                            << "HookSet[" << HS_ACC()
                            << "]: Malformed transaction: Parameter without ParameterName";
                        return tecINTERNAL;
                    }

                    ripple::Blob paramName = hookParameterObj->getFieldVL(sfHookParameterName);
                    if (paramName.size() > paramMax)
                    {
                        JLOG(viewJ.trace())
                            << "HookSet[" << HS_ACC()
                            << "]: Malformed transaction: ParameterName too large";
                        return tecINTERNAL;
                    }

                    if (hookParameterObj->isFieldPresent(sfHookParameterValue))
                    {
                        // parameter update or set operation
                        ripple::Blob newValue = hookParameterObj->getFieldVL(sfHookParameterValue);
                        if (newValue.size() > paramMax)
                        {
                            JLOG(viewJ.trace())
                                << "HookSet[" << HS_ACC()
                                << "]: Malformed transaction: ParameterValue too large";
                            return tecINTERNAL;
                        }
                        parameters[hookParameterObj->getFieldVL(sfHookParameterName)] = newValue;
                    }
                    else
                    {
                        // parameter delete operation
                        parameters.erase(hookParameterObj->getFieldVL(sfHookParameterName));
                    }
                }

                int parameterCount = (int)(parameters.size());
                if (parameterCount > 16)
                {
                    JLOG(viewJ.trace())
                        << "HookSet[" << HS_ACC()
                        << "]: Malformed transaction: Txn would result in too many parameters on hook";
                    return tecINTERNAL;
                }

                STArray newParameters {sfHookParameters, parameterCount};
                int upto = 0;
                for (const auto& [parameterName, parameterValue] : parameters)
                {
                    STObject param { sfHookParameter };
                    param.setFieldVL(sfHookParameterName, parameterName);
                    param.setFieldVL(sfHookParameterValue, parameterValue);
                    newParameters[upto++] = std::move(param);
                }

                newHook.setFieldArray(sfHookParameters, std::move(newParameters));
            }

            // process the grants
            if (hasGrants)
                newHook.setFieldArray(sfHookGrants, hookSetObj->getFieldArray(sfHookGrants));

            newHooks[hookSetNumber] = std::move(newHook);
            continue;
        }


        JLOG(viewJ.trace())
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
                JLOG(viewJ.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook operation would delete a namespace of hook states"
                    << " but to do this you must set the NSDELETE flag";
                return tecREQUIRES_FLAG;
            }

            destroyNamespace(ctx.app, view(), account_, { ltDIR_NODE, p.first } );
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
                JLOG(viewJ.trace())
                    << "HookSet[" << HS_ACC()
                    << "]: Malformed transaction: SetHook operation would delete a hook"
                    << " but to do this you must set the OVERRIDE flag";
                return tecREQUIRES_FLAG;
            }
            view().erase(sle);
        }
    }

    return tesSUCCESS;
}


}  // namespace ripple
