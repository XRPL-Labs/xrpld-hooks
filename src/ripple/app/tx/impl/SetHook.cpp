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
namespace ripple {

NotTEC
SetHook::preflight(PreflightContext const& ctx)
{
    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    printf("preflight sethook 1\n");

    if (!ctx.tx.isFieldPresent(sfCreateCode))
    {    JLOG(ctx.j.trace())
            << "Malformed transaction: Invalid signer set list format.";
        return temMALFORMED;
    }
    
    printf("preflight sethook 2\n");

    return preflight2(ctx);

    /*
    Blob hook = ctx.tx.getFieldVL(sfCreateCode);      

    if (!hook.empty()) { // if the hook is empty it's a delete request
        printf("preflight sethook 3\n");

        //todo: [RH] check wasm guest's function table to ensure only api functions are present        
        wasmer_instance_t *instance = NULL;
        if (wasmer_instantiate(&instance, hook.data(), hook.size(), {}, 0) != WASMER_OK) {
            JLOG(ctx.j.trace()) << "Tried to set a hook with invalid code.";
            return temMALFORMED;
        }
        printf("preflight sethook 4\n");

        wasmer_instance_destroy(instance);
   }

        printf("preflight sethook 5\n");

    */
}

TER
SetHook::doApply()
{
    preCompute();

    // Perform the operation preCompute() decided on.
    switch (do_)
    {
        case set:
            return replaceHook();

        case destroy:
            return destroyHook();

        default:
            break;
    }
    assert(false);  // Should not be possible to get here.
    return temMALFORMED;
}

void
SetHook::preCompute()
{
    hook_ = ctx_.tx.getFieldVL(sfCreateCode);
    do_ = ( hook_.empty() ? destroy : set );

    return Transactor::preCompute();
}


TER
SetHook::destroyEntireHookState(
    Application& app,
    ApplyView& view,
    const AccoundID& account,
    Keylet const& accountKeylet,
    Keylet const& ownerDirKeylet,
    Keylet const& hookKeylet
) {

    std::shared_ptr<SLE const> sleDirNode{};
    unsigned int uDirEntry{0};
    uint256 dirEntry{beast::zero};

    if (!cdirFirst(
            view(),
            ownerDirKeylet.key,
            sleDirNode,
            uDirEntry,
            dirEntry,
            app.journal)) {
            JLOG(app.journal("View"))
                << "SetHook (delete state): account directory missing " << account;
        return tefINTERNAL;
    }

    std::int32_t deletableDirEntryCount{0};
    do
    {
        // Make sure any directory node types that we find are the kind
        // we can delete.
        Keylet const itemKeylet{ltCHILD, dirEntry}; // todo: ??? [RH] can I just specify ltHOOK_STATE here ???
        auto sleItem = view().read(itemKeylet);
        if (!sleItem)
        {
            // Directory node has an invalid index.  Bail out.
            JLOG(app.journal.fatal())
                << "SetHook (delete state): directory node in ledger " << view().seq()
                << " has index to object that is missing: "
                << to_string(dirEntry);
            return tefBAD_LEDGER;
        }

        LedgerEntryType const nodeType{
            safe_cast<LedgerEntryType>((*sleItem)[sfLedgerEntryType])};

        if (nodeType == ltHOOK_STATE) {
            // delete it!
            // todo: [RH] check if it's safe to delete while iterating ???
            auto const hint = (*sleItem)[sfOwnerNode];
            if (!view.dirRemove(ownerDirKeylet, hint, hookKeylet.key, false))
            {
                return tefBAD_LEDGER;
            }
            view.erase(sleItem);
        }


    } while (cdirNext(
        view(), ownerDirKeylet.key, sleDirNode, uDirEntry, dirEntry, app.journal));

    return tesSUCCESS;
}

TER
SetHook::replaceHook()
{

    const int hookDataMaxSize = hook::max_hook_data; 


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
    uint32_t previousReserveUnits = ( oldHook ? oldHook->getFieldU32(sfHookCodeReserve) : 0 );

    // get the new cost to store, if any
    uint32_t newReserveUnits = std::ceil( (double)(hook_.size()) / (5.0 * (double)hookDataMaxSize) ); 

    // get existing flags
    uint32_t flags = ( oldHook ?  oldHook->getFieldU32(sfFlags) : 0 );

    if (hook_.empty() && oldHook && oldHook->getFieldVL(sfCreateCode).empty()) {
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

    int addedOwnerCount = newReserveUnits - previousReserveUnits;

    XRPAmount const newReserve{
        view().fees().accountReserve(oldOwnerCount + addedOwnerCount)};

    if (mPriorBalance < newReserve)
        return tecINSUFFICIENT_RESERVE;

    auto hook = std::make_shared<SLE>(hookKeylet);
    view().insert(hook);

    hook->setFieldVL(sfCreateCode, hook_);
    hook->setFieldU32(sfHookStateCount, stateCount);
    hook->setFieldU32(sfHookReserveCount, newReserveUnits);
    hook->setFieldU32(sfHookDataMaxSize, hookDataMaxSize); 
    hook->setFieldU32(sfFlags, flags);

    auto viewJ = ctx_.app.journal("View");
    // Add the hook to the account's directory.
    auto const page = dirAdd(
        ctx_.view(),
        ownerDirKeylet,
        hookKeylet.key,
        false,
        describeOwnerDir(account_),
        viewJ);

    JLOG(j_.trace()) << "Create hook for account " << toBase58(account_)
                     << ": " << (page ? "success" : "failure");

    if (!page)
        return tecDIR_FULL;
    
    hook->setFieldU64(sfOwnerNode, *page);

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

    adjustOwnerCount(
        view,
        view.peek(accountKeylet),
        -1,
        app.journal("View"));

    // remove the actual hook
    view.erase(hook);

    return tesSUCCESS;
}


TER
SetHook::destroyHook()
{
    auto const accountKeylet = keylet::account(account_);
    SLE::pointer ledgerEntry = view().peek(accountKeylet);
    if (!ledgerEntry)
        return tefINTERNAL;

    auto const ownerDirKeylet = keylet::ownerDir(account_);
    auto const hookKeylet = keylet::hook(account_);
    return removeHookFromLedger(
        ctx_.app, view(), accountKeylet, ownerDirKeylet, hookKeylet);
}

}  // namespace ripple
