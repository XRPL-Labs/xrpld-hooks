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

#ifndef RIPPLE_TX_SETHOOK_H_INCLUDED
#define RIPPLE_TX_SETHOOK_H_INCLUDED

#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/tx/impl/SignerEntries.h>
#include <ripple/app/tx/impl/Transactor.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/Buffer.h>
#include <ripple/basics/Blob.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STTx.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace ripple {

enum HookSetOperation : uint8_t
{
    HSO_REORDER         = 0U,
    HSO_CREATE          = 1U,
	HSO_LINK            = 2U,
	HSO_UNLINK          = 3U,
	HSO_NSSET           = 4U,
	HSO_NSMOVE          = 5U,
	HSO_NSDELETE        = 6U,
	HSO_PASET           = 7U,
	HSO_PARESET         = 8U,
	HSO_FAUTH           = 9U,
	HSO_FUNAUTH         = 10U,
	HSO_HOOKON          = 11U,
	HSO_ANNIHILATE      = 12U
};

enum HookSetFields : uint8_t
{
    HSF_OPERATION      =   1U,
    HSF_SEQUENCE       =   2U,
    HSF_REORDER        =   4U,
    HSF_ON             =   8U,
    HSF_NAMESPACE      =  16U,
    HSF_HASH           =  32U,
    HSF_PARAMETERS     =  64U,
    HSF_DEFINITION     = 128U
}

/**
See the README.md for an overview of the SetHook transaction that
this class implements.
*/
class SetHook : public Transactor
{
private:
    // Values determined during preCompute for use later.
    Blob hook_;
    uint32_t hookOn_;
public:
    explicit SetHook(ApplyContext& ctx) : Transactor(ctx)
    {
    }

    static bool
    affectsSubsequentTransactionAuth(STTx const& tx)
    {
        return true;
    }

    static NotTEC
    preflight(PreflightContext const& ctx);

    TER
    doApply() override;
    void
    preCompute() override;

private:

    TER
    setHook();

    TER
    destroyEntireHookState(
        Application& app,
        ApplyView& view,
        const AccountID& account,
        const Keylet & accountKeylet,
        const Keylet & ownerDirKeylet,
        const Keylet & hookKeylet
    );


    TER
    removeHookFromLedger(
        Application& app,
        ApplyView& view,
        Keylet const& accountKeylet,
        Keylet const& ownerDirKeylet,
        Keylet const& hookKeylet
    );

};

}  // namespace ripple

#endif
