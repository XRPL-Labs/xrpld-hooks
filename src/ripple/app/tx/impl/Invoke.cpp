//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#include <ripple/app/tx/impl/Invoke.h>
#include <ripple/basics/Log.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>

namespace ripple {

TxConsequences
Invoke::makeTxConsequences(PreflightContext const& ctx)
{
    return TxConsequences{ctx.tx, TxConsequences::normal};
}

NotTEC
Invoke::preflight(PreflightContext const& ctx)
{
    if (auto const ret = preflight1(ctx); !isTesSuccess(ret))
        return ret;

    auto& tx = ctx.tx;
    auto& j = ctx.j;

    if (tx.getFieldVL(sfBlob).size() > (128*1024))
        return temMALFORMED;

    return preflight2(ctx);
}

TER
Invoke::preclaim(PreclaimContext const& ctx)
{
    if (!ctx.view.rules().enabled(featureHooks))
        return temDISABLED;

    auto const id = ctx.tx[sfAccount];

    auto const sle = ctx.view.read(keylet::account(id));
    if (!sle)
        return terNO_ACCOUNT;

    if (ctx.tx.isFieldPresent(sfDestination))
    {
        if (!ctx.view.exists(keylet::account(ctx.tx[sfDestination])))
            return tecNO_TARGET;
    }

    return tesSUCCESS;
}

TER
Invoke::doApply()
{
    // everything happens in the hooks!
    return tesSUCCESS;
}

FeeUnit64
Invoke::calculateBaseFee(ReadView const& view, STTx const& tx)
{
    FeeUnit64 extraFee{0};

    if (tx.isFieldPresent(sfBlob))
        extraFee += FeeUnit64{ tx.getFieldVL(sfBlob).size() };

    if (tx.isFieldPresent(sfHookParameters))
    {
        uint64_t paramBytes = 0;
        auto const& params = tx.getFieldArray(sfHookParameters);
        for (auto const& param : params)
        {
            paramBytes +=
                (param.isFieldPresent(sfHookParameterName) ?
                    param.getFieldVL(sfHookParameterName).size() : 0) +
                (param.isFieldPresent(sfHookParameterValue) ?
                    param.getFieldVL(sfHookParameterValue).size() : 0);
        }
        extraFee += FeeUnit64 { paramBytes };
    }

    return Transactor::calculateBaseFee(view, tx) + extraFee;
}

}  // namespace ripple
