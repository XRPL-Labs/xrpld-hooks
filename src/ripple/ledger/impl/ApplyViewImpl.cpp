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

#include <ripple/basics/contract.h>
#include <ripple/ledger/ApplyViewImpl.h>
#include <cassert>

namespace ripple {

ApplyViewImpl::ApplyViewImpl(ReadView const* base, ApplyFlags flags)
    : ApplyViewBase(base, flags)
{
}

void
ApplyViewImpl::apply(OpenView& to, STTx const& tx, TER ter, beast::Journal j)
{
    items_.apply(to, tx, ter, deliver_, hookExecution_, j);
}

std::size_t
ApplyViewImpl::size()
{
    return items_.size();
}

void
ApplyViewImpl::visit(
    OpenView& to,
    std::function<void(
        uint256 const& key,
        bool isDelete,
        std::shared_ptr<SLE const> const& before,
        std::shared_ptr<SLE const> const& after)> const& func)
{
    items_.visit(to, func);
}

std::map<std::tuple<AccountID, AccountID, Currency>, STAmount>
ApplyView::balanceChanges(ReadView const& view) const
{
    using key_t = std::tuple<AccountID, AccountID, Currency>;
    // Map of delta trust lines. As a special case, when both ends of the trust
    // line are the same currency, then it's delta currency for that issuer. To
    // get the change in XRP balance, Account == root, issuer == root, currency
    // == XRP
    std::map<key_t, STAmount> result;

    // populate a dictionary with low/high/currency/delta. This can be
    // compared with the other versions payment code.
    auto each = [&result](
                    uint256 const& key,
                    bool isDelete,
                    std::shared_ptr<SLE const> const& before,
                    std::shared_ptr<SLE const> const& after) {
        STAmount oldBalance;
        STAmount newBalance;
        AccountID lowID;
        AccountID highID;

        // before is read from prev view
        if (isDelete)
        {
            if (!before)
                return;

            auto const bt = before->getType();
            switch (bt)
            {
                case ltACCOUNT_ROOT:
                    lowID = xrpAccount();
                    highID = (*before)[sfAccount];
                    oldBalance = (*before)[sfBalance];
                    newBalance = oldBalance.zeroed();
                    break;
                case ltRIPPLE_STATE:
                    lowID = (*before)[sfLowLimit].getIssuer();
                    highID = (*before)[sfHighLimit].getIssuer();
                    oldBalance = (*before)[sfBalance];
                    newBalance = oldBalance.zeroed();
                    break;
                case ltOFFER:
                    // TBD
                    break;
                default:
                    break;
            }
        }
        else if (!before)
        {
            // insert
            auto const at = after->getType();
            switch (at)
            {
                case ltACCOUNT_ROOT:
                    lowID = xrpAccount();
                    highID = (*after)[sfAccount];
                    newBalance = (*after)[sfBalance];
                    oldBalance = newBalance.zeroed();
                    break;
                case ltRIPPLE_STATE:
                    lowID = (*after)[sfLowLimit].getIssuer();
                    highID = (*after)[sfHighLimit].getIssuer();
                    newBalance = (*after)[sfBalance];
                    oldBalance = newBalance.zeroed();
                    break;
                case ltOFFER:
                    // TBD
                    break;
                default:
                    break;
            }
        }
        else
        {
            // modify
            auto const at = after->getType();
            assert(at == before->getType());
            switch (at)
            {
                case ltACCOUNT_ROOT:
                    lowID = xrpAccount();
                    highID = (*after)[sfAccount];
                    oldBalance = (*before)[sfBalance];
                    newBalance = (*after)[sfBalance];
                    break;
                case ltRIPPLE_STATE:
                    lowID = (*after)[sfLowLimit].getIssuer();
                    highID = (*after)[sfHighLimit].getIssuer();
                    oldBalance = (*before)[sfBalance];
                    newBalance = (*after)[sfBalance];
                    break;
                case ltOFFER:
                    // TBD
                    break;
                default:
                    break;
            }
        }
        // The following are now set, put them in the map
        auto delta = newBalance - oldBalance;
        auto const cur = newBalance.getCurrency();
        result[std::make_tuple(lowID, highID, cur)] = delta;
        auto r = result.emplace(std::make_tuple(lowID, lowID, cur), delta);
        if (r.second)
        {
            r.first->second += delta;
        }

        delta.negate();
        r = result.emplace(std::make_tuple(highID, highID, cur), delta);
        if (r.second)
        {
            r.first->second += delta;
        }
    };
    items_.visit(view, each);
    return result;
}

}  // namespace ripple
