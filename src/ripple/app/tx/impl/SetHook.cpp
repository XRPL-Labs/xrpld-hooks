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
#include <ripple/app/tx/applyHook.h>

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
#include <ripple/app/tx/applyHook.h>
namespace ripple {

// web assembly contains a lot of run length encoding in LEB128 format
inline int parseLeb128(std::vector<unsigned char>& buf, int start_offset, int* end_offset)
{
    int val = 0, shift = 0, i = start_offset;

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
}

NotTEC
SetHook::preflight(PreflightContext const& ctx)
{
    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    printf("preflight sethook 1\n");

    if (!ctx.tx.isFieldPresent(sfCreateCode) ||
        !ctx.tx.isFieldPresent(sfHookOn))
    {   
        JLOG(ctx.j.trace())
            << "Malformed transaction: Invalid SetHook format.";
        return temMALFORMED;
    }
    
    printf("preflight sethook 2\n");

    Blob hook = ctx.tx.getFieldVL(sfCreateCode);      

    // if the hook is empty it's a delete request
    if (!hook.empty()) { 
        printf("preflight sethook 3\n");

        if (hook.size() < 10)
        {
            JLOG(ctx.j.trace())
                << "Malformed transaction: Hook was not valid webassembly binary. Too small.";
            return temMALFORMED;
        }

        unsigned char header[8] = { 0x00U, 0x61U, 0x73U, 0x6DU, 0x01U, 0x00U, 0x00U, 0x00U };

        // check header, magic number
        // RH TODO clean up debug output
        bool bad_header = false;
        {
            printf("Hook start: `");
            for (int i = 0; i < 8; ++i)
            {
                bool match = hook[i] == header[i];
                printf("%02X - %s, ", hook[i], (match ? "t" : "f"));
                if (!match) bad_header = true;
            }
            printf("`\n");
        }
        if (bad_header)
        {
            JLOG(ctx.j.trace())
                << "Malformed transaction: Hook was not valid webassembly binary. missing magic number or version.";
            return temMALFORMED;
        }

        for (int i = 8, j = 0; i < hook.size();)
        {

            if (j == i)
            {
                // if the loop iterates twice with the same value for i then
                // it's an infinite loop edge case
                JLOG(ctx.j.trace())
                    << "Malformed transaction: Hook is invalid WASM binary.";
                return temMALFORMED;                
            }
            
            j = i;

            int section_type = hook[i++];
            int section_length = parseLeb128(hook, i, &i);
            int section_start = i;


            printf("WASM binary analysis -- upto %d: section %d with length %d\n", i, section_type, section_length);
            if (section_type != 7)
            {
                i += section_length;
                continue;
            }

            // execution to here means we are inside the export section
            int export_count = parseLeb128(hook, i, &i);
            if (export_count <= 0)
            {
                JLOG(ctx.j.trace())
                    << "Malformed transaction: Hook did not export any functions... "
                    "required hook(int64_t), callback(int64_t).";
                return temMALFORMED;
            }

            bool found_hook_export = false;
            bool found_cbak_export = false;
            for (int j = 0; j < export_count && !(found_hook_export && found_cbak_export); ++j)
            {
                int name_len = parseLeb128(hook, i, &i);
                if (name_len == 4) 
                {

                    if (hook[i] == 'h' && hook[i+1] == 'o' && hook[i+2] == 'o' && hook[i+3] == 'k')
                        found_hook_export = true;
                    else
                    if (hook[i] == 'c' && hook[i+1] == 'b' && hook[i+2] == 'a' && hook[i+3] == 'k')
                        found_cbak_export = true;
                }

                i += name_len + 1;
                parseLeb128(hook, i, &i);

            }

            // execution to here means export section was parsed
            if (!(found_hook_export && found_cbak_export))
            {
                JLOG(ctx.j.trace())
                    << "Malformed transaction: Hook did not export: " <<
                    ( !found_hook_export ? "hook(int64_t); " : "" ) <<
                    ( !found_cbak_export ? "cbak(int64_t);"  : "" );
                return temMALFORMED;
            }
            break;
        }

        // check if wasmer can run it
        wasmer_instance_t *instance = NULL;
        if (wasmer_instantiate(
            &instance, hook.data(), hook.size(), hook::imports, hook::imports_count) 
                != wasmer_result_t::WASMER_OK) {
            hook::printWasmerError();
            JLOG(ctx.j.trace()) << "Tried to set a hook with invalid code.";
            return temMALFORMED;
        }        
        
        wasmer_instance_destroy(instance);

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
    hook_ = ctx_.tx.getFieldVL(sfCreateCode);
    hookOn_ = ctx_.tx.getFieldU64(sfHookOn);
    return Transactor::preCompute();
}



TER
SetHook::destroyEntireHookState(
    Application& app,
    ApplyView& view,
    const AccountID& account,
    const Keylet & accountKeylet,
    const Keylet & ownerDirKeylet,
    const Keylet & hookKeylet
) {

    printf("destroyEntireHookState called\n");

    std::shared_ptr<SLE const> sleDirNode{};
    unsigned int uDirEntry{0};
    uint256 dirEntry{beast::zero};
    
    if (dirIsEmpty(view, ownerDirKeylet))
        return tesSUCCESS;

    auto j = app.journal("View");
    if (!cdirFirst(
            view,
            ownerDirKeylet.key,
            sleDirNode,
            uDirEntry,
            dirEntry,
            j)) {
            JLOG(j.fatal())
                << "SetHook (delete state): account directory missing " << account;
        return tefINTERNAL;
    }

    do
    {
        // Make sure any directory node types that we find are the kind
        // we can delete.
        Keylet const itemKeylet{ltCHILD, dirEntry}; // todo: ??? [RH] can I just specify ltHOOK_STATE here ???
        auto sleItem = view.peek(itemKeylet);
        if (!sleItem)
        {
            // Directory node has an invalid index.  Bail out.
            JLOG(j.fatal())
                << "SetHook (delete state): directory node in ledger " << view.seq()
                << " has index to object that is missing: "
                << to_string(dirEntry);
            return tefBAD_LEDGER;
        }


        auto nodeType = sleItem->getFieldU16(sfLedgerEntryType);

        printf("destroyEntireHookState iterator: %d\n", nodeType);

        if (nodeType == ltHOOK_STATE) {
            // delete it!
            // todo: [RH] check if it's safe to delete while iterating ???
            auto const hint = (*sleItem)[sfOwnerNode];
            if (!view.dirRemove(ownerDirKeylet, hint, itemKeylet.key, false))
            {
                return tefBAD_LEDGER;
            }
            view.erase(sleItem);
        }


    } while (cdirNext(
        view, ownerDirKeylet.key, sleDirNode, uDirEntry, dirEntry, j));

    return tesSUCCESS;
}

TER
SetHook::setHook()
{

    const int blobMax = hook::maxHookDataSize(); 


    auto const accountKeylet = keylet::account(account_);
    auto const ownerDirKeylet = keylet::ownerDir(account_);
    auto const hookKeylet = keylet::hook(account_);

    // This may be either a create or a replace.  Preemptively remove any
    // old hook.  May reduce the reserve, so this is done before
    // checking the reserve.

    auto const oldHook = view().peek(hookKeylet);
    
    // get the current state count, if any
    uint32_t stateCount = ( oldHook ? oldHook->getFieldU32(sfHookStateCount) : 0 );
   
    // get the previously reserved amount, if any 
    int64_t previousReserveUnits = ( oldHook ? oldHook->getFieldU32(sfHookReserveCount) : 0 );

    // get the new cost to store, if any
    int64_t newReserveUnits = std::ceil( (double)(hook_.size()) / (5.0 * (double)blobMax) ); 

    
    printf("hook empty? %s\n", ( hook_.empty() ? "yes" : "no" ));
    printf("old hook present? %s\n", ( oldHook ? "yes" : "no" ));
    printf("tx sfCreateCode empty?? %s\n", ( (oldHook) && oldHook->getFieldVL(sfCreateCode).empty() ? "yes" : "no" ));

    if (hook_.empty() && !oldHook) {
        // this is a special case for destroying the existing state data of a previously removed contract
        if (TER const ter = 
                destroyEntireHookState(ctx_.app, view(), account_, accountKeylet, ownerDirKeylet, hookKeylet))
            return ter;
        return tesSUCCESS;
    }

    // remove the existing hook object in anticipation of readding
    if (TER const ter = removeHookFromLedger(
            ctx_.app, view(), accountKeylet, ownerDirKeylet, hookKeylet))
        return ter;

    auto const sle = view().peek(accountKeylet);
    if (!sle)
        return tefINTERNAL;


    // Compute new reserve.  Verify the account has funds to meet the reserve.
    std::uint32_t const oldOwnerCount{(*sle)[sfOwnerCount]};

    int64_t addedOwnerCount = newReserveUnits - previousReserveUnits;

    printf("newReserveUnits: %d\n", newReserveUnits);
    printf("prevReserveUnits: %d\n", previousReserveUnits);
    printf("hook data size: %d\n", hook_.size());

//    if (addedOwnerCount >= (1ULL<<32))
//        return tefINTERNAL;

    XRPAmount const newReserve{
        view().fees().accountReserve(oldOwnerCount + addedOwnerCount)};

    if (mPriorBalance < newReserve)
        return tecINSUFFICIENT_RESERVE;
    
    auto viewJ = ctx_.app.journal("View");

    if (!hook_.empty()) {
        auto hook = std::make_shared<SLE>(hookKeylet);
        view().insert(hook);

        hook->setAccountID(sfAccount, account_);
        hook->setFieldVL(sfCreateCode, hook_);
        hook->setFieldU32(sfHookStateCount, stateCount);
        hook->setFieldU32(sfHookReserveCount, newReserveUnits);
        hook->setFieldU32(sfHookDataMaxSize, blobMax); 
        hook->setFieldU64(sfHookOn, hookOn_); 

        // Add the hook to the account's directory.
        auto const page = dirAdd(
            ctx_.view(),
            ownerDirKeylet,
            hookKeylet.key,
            false,
            describeOwnerDir(account_),
            viewJ);

        JLOG(viewJ.trace()) << "Create hook for account " << toBase58(account_)
                         << ": " << (page ? "success" : "failure");

        if (!page)
            return tecDIR_FULL;
        hook->setFieldU64(sfOwnerNode, *page);
    }

    printf("adjust owner count on sethook %d\n", addedOwnerCount);
    fflush(stdout);

    adjustOwnerCount(view(), sle, addedOwnerCount, viewJ);
    return tesSUCCESS;
}

TER
SetHook::removeHookFromLedger(
    Application& app,
    ApplyView& view,
    Keylet const& accountKeylet,
    Keylet const& ownerDirKeylet,
    Keylet const& hookKeylet)
{
    SLE::pointer hook = view.peek(hookKeylet);

    // If the signer list doesn't exist we've already succeeded in deleting it.
    if (!hook)
        return tesSUCCESS;

    // Remove the node from the account directory.
    auto const hint = (*hook)[sfOwnerNode];

    //todo: [RH] we probably don't need to delete this every time
    if (!view.dirRemove(ownerDirKeylet, hint, hookKeylet.key, false))
    {
        return tefBAD_LEDGER;
    }

    // remove the actual hook
    view.erase(hook);

    return tesSUCCESS;
}


}  // namespace ripple
