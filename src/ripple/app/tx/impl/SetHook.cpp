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
#include <ripple/basics/Hook.h>

#include <ripple/ledger/ApplyView.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STTx.h>
#include <algorithm>
#include <cstdint>

namespace ripple {

NotTEC
SetHook::preflight(PreflightContext const& ctx)
{
    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    if (!ctx.tx.isFieldPresent(sfCreateCode))
        JLOG(ctx.j.trace())
            << "Malformed transaction: Invalid signer set list format.";
        return temMALFORMED;

    Blob hook = ctx_.tx.getFieldVL(sfCreateCode);      

    if (!hook.empty()) { // if the hook is empty it's a delete request
        TER hook_validation_result = validateHook(hook, ctx.j);
        if (hook_validation_result != tesSUCCESS)
            return hook_validation_result;
    }

    return preflight2(ctx);
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
    do_ = ( hook_.empty() ? Operation.destroy : Operation.set );

    return Transactor::preCompute();
}

static TER
removeHookFromLedger(
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
SetHook::removeFromLedger(
    Application& app,
    ApplyView& view,
    AccountID const& account)
{
    auto const accountKeylet = keylet::account(account);
    auto const ownerDirKeylet = keylet::ownerDir(account);
    auto const hookKeylet = keylet::hook(account);

    return removeHookFromLedger(
        app, view, accountKeylet, ownerDirKeylet, hookKeylet);
}

SetHook::validateHook(
    const Blob& code,
    beast::Journal j)
{
    
    if (code.empty()) {
        JLOG(j.trace()) << "Tried to set a hook with empty / null wasm code.";
        return temMALFORMED;
    }

    wasmer_instance_t *instance = NULL;
    if (wasmer_instantiate(&instance, code.data(), code.size(), (wasmer_import_t imports[]){}, 0) != WASMER_OK) {
        JLOG(j.trace()) << "Tried to set a hook with invalid code.";
        return temMALFORMED;
    }

    //TODO: [RH] detect if a hook() call with the correct signature is present in the provided wasm

    wasmer_instance_destroy(instance);
    
    return tesSUCCESS;
}

TER
SetHook::replaceHook()
{
    auto const accountKeylet = keylet::account(account_);
    auto const ownerDirKeylet = keylet::ownerDir(account_);
    auto const hookKeylet = keylet::hook(account_);

    // This may be either a create or a replace.  Preemptively remove any
    // old hook.  May reduce the reserve, so this is done before
    // checking the reserve.
    if (TER const ter = removeHookFromLedger(
            ctx_.app, view(), accountKeylet, ownerDirKeylet, hookKeylet))
        return ter;

    auto const sle = view().peek(accountKeylet);
    if (!sle)
        return tefINTERNAL;

    // Compute new reserve.  Verify the account has funds to meet the reserve.
    std::uint32_t const oldOwnerCount{(*sle)[sfOwnerCount]};


    int addedOwnerCount{1};

    XRPAmount const newReserve{
        view().fees().accountReserve(oldOwnerCount + addedOwnerCount)};

    // We check the reserve against the starting balance because we want to
    // allow dipping into the reserve to pay fees.  This behavior is consistent
    // with CreateTicket.
    if (mPriorBalance < newReserve)
        return tecINSUFFICIENT_RESERVE;

    // Everything's ducky.  Add the ltHOOK to the ledger.
    auto hook = std::make_shared<SLE>(hookKeylet);
    view().insert(hook);
    writeHookToSLE(hook, flags);

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

    // If we succeeded, the new entry counts against the
    // creator's reserve.
    adjustOwnerCount(view(), sle, addedOwnerCount, viewJ);
    return tesSUCCESS;
}

TER
SetHook::destroyHook()
{
    auto const accountKeylet = keylet::account(account_);
    // Destroying the signer list is only allowed if either the master key
    // is enabled or there is a regular key.
    SLE::pointer ledgerEntry = view().peek(accountKeylet);
    if (!ledgerEntry)
        return tefINTERNAL;

    if ((ledgerEntry->isFlag(lsfDisableMaster)) &&
        (!ledgerEntry->isFieldPresent(sfRegularKey)))
        return tecNO_ALTERNATIVE_KEY;

    auto const ownerDirKeylet = keylet::ownerDir(account_);
    auto const hookKeylet = keylet::hook(account_);
    return removeSignersFromLedger(
        ctx_.app, view(), accountKeylet, ownerDirKeylet, hookKeylet);
}

void
SetHook::writeHookToSLE(
    SLE::pointer const& ledgerEntry,
    std::uint32_t flags) const
{
    //todo: support flags?

    auto toLegder = std::make_unique<STBlob>(sfCreateCode, hook_.data(), hook_.size());

    ledgerEntry->setFieldVL(sfCreateCode, toLedger);
}

}  // namespace ripple
