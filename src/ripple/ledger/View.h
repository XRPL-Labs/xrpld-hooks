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

#ifndef RIPPLE_LEDGER_VIEW_H_INCLUDED
#define RIPPLE_LEDGER_VIEW_H_INCLUDED

#include <ripple/beast/utility/Journal.h>
#include <ripple/core/Config.h>
#include <ripple/ledger/ApplyView.h>
#include <ripple/ledger/OpenView.h>
#include <ripple/ledger/RawView.h>
#include <ripple/ledger/ReadView.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/Rate.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/TER.h>
#include <ripple/protocol/Feature.h>
#include <ripple/basics/Log.h>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <type_traits>
#include <vector>

namespace ripple {

//------------------------------------------------------------------------------
//
// Observers
//
//------------------------------------------------------------------------------

/** Controls the treatment of frozen account balances */
enum FreezeHandling { fhIGNORE_FREEZE, fhZERO_IF_FROZEN };

[[nodiscard]] bool
isGlobalFrozen(ReadView const& view, AccountID const& issuer);

[[nodiscard]] bool
isFrozen(
    ReadView const& view,
    AccountID const& account,
    Currency const& currency,
    AccountID const& issuer);

// Returns the amount an account can spend without going into debt.
//
// <-- saAmount: amount of currency held by account. May be negative.
[[nodiscard]] STAmount
accountHolds(
    ReadView const& view,
    AccountID const& account,
    Currency const& currency,
    AccountID const& issuer,
    FreezeHandling zeroIfFrozen,
    beast::Journal j);

[[nodiscard]] STAmount
accountFunds(
    ReadView const& view,
    AccountID const& id,
    STAmount const& saDefault,
    FreezeHandling freezeHandling,
    beast::Journal j);

// Return the account's liquid (not reserved) XRP.  Generally prefer
// calling accountHolds() over this interface.  However this interface
// allows the caller to temporarily adjust the owner count should that be
// necessary.
//
// @param ownerCountAdj positive to add to count, negative to reduce count.
[[nodiscard]] XRPAmount
xrpLiquid(
    ReadView const& view,
    AccountID const& id,
    std::int32_t ownerCountAdj,
    beast::Journal j);

/** Iterate all items in an account's owner directory. */
void
forEachItem(
    ReadView const& view,
    AccountID const& id,
    std::function<void(std::shared_ptr<SLE const> const&)> f);

/** Iterate all items after an item in an owner directory.
    @param after The key of the item to start after
    @param hint The directory page containing `after`
    @param limit The maximum number of items to return
    @return `false` if the iteration failed
*/
bool
forEachItemAfter(
    ReadView const& view,
    AccountID const& id,
    uint256 const& after,
    std::uint64_t const hint,
    unsigned int limit,
    std::function<bool(std::shared_ptr<SLE const> const&)> f);

[[nodiscard]] Rate
transferRate(ReadView const& view, AccountID const& issuer);

/** Returns `true` if the directory is empty
    @param key The key of the directory
*/
[[nodiscard]] bool
dirIsEmpty(ReadView const& view, Keylet const& k);

// Return the list of enabled amendments
[[nodiscard]] std::set<uint256>
getEnabledAmendments(ReadView const& view);

// Return a map of amendments that have achieved majority
using majorityAmendments_t = std::map<uint256, NetClock::time_point>;
[[nodiscard]] majorityAmendments_t
getMajorityAmendments(ReadView const& view);

/** Return the hash of a ledger by sequence.
    The hash is retrieved by looking up the "skip list"
    in the passed ledger. As the skip list is limited
    in size, if the requested ledger sequence number is
    out of the range of ledgers represented in the skip
    list, then std::nullopt is returned.
    @return The hash of the ledger with the
            given sequence number or std::nullopt.
*/
[[nodiscard]] std::optional<uint256>
hashOfSeq(ReadView const& ledger, LedgerIndex seq, beast::Journal journal);

/** Find a ledger index from which we could easily get the requested ledger

    The index that we return should meet two requirements:
        1) It must be the index of a ledger that has the hash of the ledger
            we are looking for. This means that its sequence must be equal to
            greater than the sequence that we want but not more than 256 greater
            since each ledger contains the hashes of the 256 previous ledgers.

        2) Its hash must be easy for us to find. This means it must be 0 mod 256
            because every such ledger is permanently enshrined in a LedgerHashes
            page which we can easily retrieve via the skip list.
*/
inline LedgerIndex
getCandidateLedger(LedgerIndex requested)
{
    return (requested + 255) & (~255);
}

/** Return false if the test ledger is provably incompatible
    with the valid ledger, that is, they could not possibly
    both be valid. Use the first form if you have both ledgers,
    use the second form if you have not acquired the valid ledger yet
*/
[[nodiscard]] bool
areCompatible(
    ReadView const& validLedger,
    ReadView const& testLedger,
    beast::Journal::Stream& s,
    const char* reason);

[[nodiscard]] bool
areCompatible(
    uint256 const& validHash,
    LedgerIndex validIndex,
    ReadView const& testLedger,
    beast::Journal::Stream& s,
    const char* reason);

//------------------------------------------------------------------------------
//
// Modifiers
//
//------------------------------------------------------------------------------

/** Adjust the owner count up or down. */
void
adjustOwnerCount(
    ApplyView& view,
    std::shared_ptr<SLE> const& sle,
    std::int32_t amount,
    beast::Journal j);

/** @{ */
/** Returns the first entry in the directory, advancing the index

    @deprecated These are legacy function that are considered deprecated
                and will soon be replaced with an iterator-based model
                that is easier to use. You should not use them in new code.

    @param view The view against which to operate
    @param root The root (i.e. first page) of the directory to iterate
    @param page The current page
    @param index The index inside the current page
    @param entry The entry at the current index

    @return true if the directory isn't empty; false otherwise
 */
bool
cdirFirst(
    ReadView const& view,
    uint256 const& root,
    std::shared_ptr<SLE const>& page,
    unsigned int& index,
    uint256& entry);

bool
dirFirst(
    ApplyView& view,
    uint256 const& root,
    std::shared_ptr<SLE>& page,
    unsigned int& index,
    uint256& entry);
/** @} */

/** @{ */
/** Returns the next entry in the directory, advancing the index

    @deprecated These are legacy function that are considered deprecated
                and will soon be replaced with an iterator-based model
                that is easier to use. You should not use them in new code.

    @param view The view against which to operate
    @param root The root (i.e. first page) of the directory to iterate
    @param page The current page
    @param index The index inside the current page
    @param entry The entry at the current index

    @return true if the directory isn't empty; false otherwise
 */
bool
cdirNext(
    ReadView const& view,
    uint256 const& root,
    std::shared_ptr<SLE const>& page,
    unsigned int& index,
    uint256& entry);

bool
dirNext(
    ApplyView& view,
    uint256 const& root,
    std::shared_ptr<SLE>& page,
    unsigned int& index,
    uint256& entry);
/** @} */

[[nodiscard]] std::function<void(SLE::ref)>
describeOwnerDir(AccountID const& account);

// VFALCO NOTE Both STAmount parameters should just
//             be "Amount", a unit-less number.
//
/** Create a trust line

    This can set an initial balance.
*/
[[nodiscard]] TER
trustCreate(
    ApplyView& view,
    const bool bSrcHigh,
    AccountID const& uSrcAccountID,
    AccountID const& uDstAccountID,
    uint256 const& uIndex,      // --> ripple state entry
    SLE::ref sleAccount,        // --> the account being set.
    const bool bAuth,           // --> authorize account.
    const bool bNoRipple,       // --> others cannot ripple through
    const bool bFreeze,         // --> funds cannot leave
    STAmount const& saBalance,  // --> balance of account being set.
                                // Issuer should be noAccount()
    STAmount const& saLimit,    // --> limit for account being set.
                                // Issuer should be the account being set.
    std::uint32_t uSrcQualityIn,
    std::uint32_t uSrcQualityOut,
    beast::Journal j);

[[nodiscard]] TER
trustDelete(
    ApplyView& view,
    std::shared_ptr<SLE> const& sleRippleState,
    AccountID const& uLowAccountID,
    AccountID const& uHighAccountID,
    beast::Journal j);

bool isTrustDefault(
    std::shared_ptr<SLE> const& acc,
    std::shared_ptr<SLE> const& line);

template<class V, class S>
[[nodiscard]] TER
trustAdjustLockedBalance(
    V& view,
    S& sleLine,
    STAmount const& deltaAmt,
    bool dryRun)
{

    static_assert(
        (std::is_same<V, ReadView const>::value &&
         std::is_same<S, std::shared_ptr<SLE const>>::value) ||
        (std::is_same<V, ApplyView>::value &&
         std::is_same<S, std::shared_ptr<SLE>>::value));

    // dry runs are explicit in code, but really the view type determines
    // what occurs here, so this combination is invalid.

    assert(!(std::is_same<V, ReadView const>::value && !dryRun));

    if (!view.rules().enabled(featurePaychanAndEscrowForTokens))
        return tefINTERNAL;

    auto const currency = deltaAmt.getCurrency();
    auto const issuer   = deltaAmt.getIssuer();

    STAmount lowLimit  = sleLine->getFieldAmount(sfLowLimit);

    // the account which is modifying the LockedBalance is always
    // the side that isn't the issuer, so if the low side is the
    // issuer then the high side is the account.
    bool high =  lowLimit.getIssuer() == issuer;

    std::vector<AccountID> parties 
        {high ? sleLine->getFieldAmount(sfHighLimit).getIssuer(): lowLimit.getIssuer()}; 

    // check for freezes & auth
    if (TER result =
        trustTransferAllowed(
            view,
            parties,
            deltaAmt.issue());
        result != tesSUCCESS)
    {
        printf("trustTransferAllowed failed on trustAdjustLockedBalance\n");
        return result;
    }

    // pull the TL balance from the account's perspective
    STAmount balance =
        high ? -(*sleLine)[sfBalance] : (*sleLine)[sfBalance];

    // this would mean somehow the issuer is trying to lock balance
    if (balance < beast::zero)
        return tecINTERNAL;

    // can't lock or unlock a zero balance
    if (balance == beast::zero)
        return tecUNFUNDED_PAYMENT;

    STAmount lockedBalance {sfLockedBalance, deltaAmt.issue()};
    if (sleLine->isFieldPresent(sfLockedBalance))
        lockedBalance = 
            high ? -(*sleLine)[sfLockedBalance] : (*sleLine)[sfLockedBalance];

    lockedBalance += deltaAmt;

    if (lockedBalance > balance)
        return tecUNFUNDED_PAYMENT;

    if (lockedBalance < beast::zero)
        return tecINTERNAL;

    // we won't update any SLEs if it is a dry run
    if (dryRun)
        return tesSUCCESS;

    if constexpr(std::is_same<V, ApplyView>::value && std::is_same<S, std::shared_ptr<SLE>>::value)
    {
        if (lockedBalance == beast::zero)
            sleLine->makeFieldAbsent(sfLockedBalance);
        else
            sleLine->
                setFieldAmount(sfLockedBalance, high ? -lockedBalance : lockedBalance);
    
        view.update(sleLine);
    }

    return tesSUCCESS;
}


// Check if movement of a particular token between 1 or more accounts
// (including unlocking) is forbidden by any flag or condition.
// If parties contains 1 entry then noRipple is not a bar to xfer.
// Part of featurePaychanAndEscrowForTokens, but can be callled without guard

template<class V>
[[nodiscard]]TER
trustTransferAllowed(
    V& view,
    std::vector<AccountID> const& parties,
    Issue const& issue)
{
    static_assert(
        std::is_same<V, ReadView const>::value ||
        std::is_same<V, ApplyView>::value);
    
    typedef typename std::conditional<
        std::is_same<V, ApplyView>::value,
        std::shared_ptr<SLE>,
        std::shared_ptr<SLE const>>::type SLEPtr;


    if (isFakeXRP(issue.currency))
        return tecNO_PERMISSION;

    auto const sleIssuerAcc = view.read(keylet::account(issue.account));

    bool lockedBalanceAllowed =
        view.rules().enabled(featurePaychanAndEscrowForTokens);

    // missing issuer is always a bar to xfer
    if (!sleIssuerAcc)
        return tecNO_ISSUER;

    // issuer global freeze is always a bar to xfer
    if (isGlobalFrozen(view, issue.account))
        return tecFROZEN;

    uint32_t issuerFlags = sleIssuerAcc->getFieldU32(sfFlags);

    bool requireAuth = issuerFlags & lsfRequireAuth;

    for (AccountID const& p: parties)
    {
        auto const line = view.read(keylet::line(p, issue.account, issue.currency));
        if (!line)
        {
            if (requireAuth)
            {
                // the line doesn't exist, i.e. it is in default state
                // default state means the line has not been authed
                // therefore if auth is required by issuer then
                // this is now a bar to xfer
                return tecNO_AUTH;
            }

            // missing line is a line in default state, this is not
            // a general bar to xfer, however additional conditions
            // do attach to completing an xfer into a default line
            // but these are checked in trustTransferLockedBalance at
            // the point of transfer.
            continue;
        }

        // sanity check the line, insane lines are a bar to xfer
        {
            // these "strange" old lines, if they even exist anymore are
            // always a bar to xfer
            if (line->getFieldAmount(sfLowLimit).getIssuer() ==
                    line->getFieldAmount(sfHighLimit).getIssuer())
                return tecINTERNAL;

            if (line->isFieldPresent(sfLockedBalance))
            {
                if (!lockedBalanceAllowed)
                {
                    printf("lockedBalanceAllowed was false\n");
                    return tecINTERNAL;
                }

                STAmount lockedBalance = line->getFieldAmount(sfLockedBalance);
                STAmount balance = line->getFieldAmount(sfBalance);

                if (lockedBalance.getCurrency() != balance.getCurrency())
                {
                    printf("lockedBalance issuer/currency did not match balance issuer/currency\n");
                    return tecINTERNAL;
                }
            }
        }

        // check the bars to xfer ... these are:
        // any TL in the set has noRipple on the issuer's side
        // any TL in the set has a freeze on the issuer's side
        // any TL in the set has RequireAuth and the TL lacks lsf*Auth
        {
            bool pHigh = p > issue.account;

            auto const flagIssuerNoRipple { pHigh ? lsfLowNoRipple : lsfHighNoRipple };
            auto const flagIssuerFreeze   { pHigh ? lsfLowFreeze   : lsfHighFreeze   };
            auto const flagIssuerAuth     { pHigh ? lsfLowAuth     : lsfHighAuth     };

            uint32_t flags = line->getFieldU32(sfFlags);

            if (flags & flagIssuerFreeze)
            {
                printf("trustTransferAllowed: issuerFreeze\n");
                return tecFROZEN;
            }

            // if called with more than one party then any party
            // that has a noripple on the issuer side of their tl
            // blocks any possible xfer
            if (parties.size() > 1 && (flags & flagIssuerNoRipple))
            {
                printf("trustTransferAllowed: issuerNoRipple\n");
                return tecPATH_DRY;
            }

            // every party involved must be on an authed trustline if
            // the issuer has specified lsfRequireAuth
            if (requireAuth && !(flags & flagIssuerAuth))
            {
                printf("trustTransferAllowed: issuerRequireAuth\n");
                return tecNO_AUTH;
            }
        }
    }

    return tesSUCCESS;
}

template <class V, class S>
[[nodiscard]] TER
trustTransferLockedBalance(
    V& view,
    AccountID const& actingAccID, // the account whose tx is actioning xfer
    S& sleSrcAcc,
    S& sleDstAcc,
    STAmount const& amount,     // issuer, currency are in this field
    beast::Journal const& j,
    bool dryRun)
{
    
    typedef typename std::conditional<
        std::is_same<V, ApplyView>::value,
        std::shared_ptr<SLE>,
        std::shared_ptr<SLE const>>::type SLEPtr;

    auto peek = [&](Keylet& k)
    {
        if constexpr (std::is_same<V, ApplyView>::value)
            return const_cast<ApplyView&>(view).peek(k);
        else
            return view.read(k);
    };

    assert(!(std::is_same<V, ApplyView>::value && !dryRun));

    if (!view.rules().enabled(featurePaychanAndEscrowForTokens))
        return tefINTERNAL;

    if (!sleSrcAcc || !sleDstAcc)
    {
        JLOG(j.warn())
            << "trustTransferLockedBalance without sleSrc/sleDst";
        return tecINTERNAL;
    }

    if (amount <= beast::zero)
    {
        JLOG(j.warn())
            << "trustTransferLockedBalance with non-positive amount";
        return tecINTERNAL;
    }

    auto issuerAccID = amount.getIssuer();
    auto currency = amount.getCurrency();
    auto srcAccID = sleSrcAcc->getAccountID(sfAccount);
    auto dstAccID = sleDstAcc->getAccountID(sfAccount);

    bool srcHigh = srcAccID > issuerAccID;
    bool dstHigh = dstAccID > issuerAccID;

    // check for freezing, auth, no ripple and TL sanity
    if (TER result =
            trustTransferAllowed(view, {srcAccID, dstAccID}, {currency, issuerAccID});
            result != tesSUCCESS)
        return result;

    // ensure source line exists
    Keylet klSrcLine { keylet::line(srcAccID, issuerAccID, currency)};
    SLEPtr sleSrcLine = peek(klSrcLine);

    if (!sleSrcLine)
        return tecNO_LINE;

    // can't transfer a locked balance that does not exist
    if (!sleSrcLine->isFieldPresent(sfLockedBalance))
    {
        JLOG(j.trace())
            << "trustTransferLockedBalance could not find sfLockedBalance on source line";
        return tecUNFUNDED_PAYMENT;
    }

    STAmount lockedBalance = sleSrcLine->getFieldAmount(sfLockedBalance);

    // check they have sufficient funds
    if (amount > lockedBalance)
        return tecUNFUNDED_PAYMENT;

    // decrement source balance
    {
        STAmount priorBalance =
            srcHigh ? -((*sleSrcLine)[sfBalance]) : (*sleSrcLine)[sfBalance];

        // ensure the currency/issuer in the locked balance matches the xfer amount
        if (priorBalance.getIssuer() != issuerAccID || priorBalance.getCurrency() != currency)
            return tecNO_PERMISSION;

        STAmount finalBalance = priorBalance - amount;

        STAmount priorLockedBalance =
            srcHigh ? -((*sleSrcLine)[sfLockedBalance]) : (*sleSrcLine)[sfLockedBalance];

        STAmount finalLockedBalance = priorLockedBalance - amount;

        // this should never happen but defensively check it here before updating sle
        if (finalBalance < beast::zero || finalLockedBalance < beast::zero)
        {
            JLOG(j.warn())
                << "trustTransferLockedBalance results in a negative balance on source line";
            return tecINTERNAL;
        }

        sleSrcLine->setFieldAmount(sfBalance, srcHigh ? -finalBalance : finalBalance);

        if (finalLockedBalance == beast::zero)
            sleSrcLine->makeFieldAbsent(sfLockedBalance);
        else
            sleSrcLine->setFieldAmount(sfLockedBalance, srcHigh ? -finalLockedBalance : finalLockedBalance);

    }

    // dstLow XNOR srcLow tells us if we need to flip the balance amount
    // on the destination line
    bool flipDstAmt = !((dstHigh && srcHigh) || (!dstHigh && !srcHigh));

    // compute transfer fee, if any
    auto xferRate = transferRate(view, issuerAccID);

    // the destination will sometimes get less depending on xfer rate
    // with any difference in tokens burned
    auto dstAmt =
        xferRate == parityRate
            ? amount
            : multiplyRound(amount, xferRate, amount.issue(), true);

    // check for a destination line
    Keylet klDstLine = keylet::line(dstAccID, issuerAccID, currency);
    SLEPtr sleDstLine = peek(klDstLine);

    if (!sleDstLine)
    {
        // in most circumstances a missing destination line is a deal breaker
        if (actingAccID != dstAccID && srcAccID != dstAccID)
            return tecNO_PERMISSION;

        STAmount dstBalanceDrops = sleDstAcc->getFieldAmount(sfBalance);

        // no dst line exists, we might be able to create one...
        if (std::uint32_t const ownerCount = {sleDstAcc->at(sfOwnerCount)};
            dstBalanceDrops < view.fees().accountReserve(ownerCount + 1))
            return tecNO_LINE_INSUF_RESERVE;

        // yes we can... we will 

        if (!dryRun)
        {
            if constexpr(std::is_same<V, ApplyView>::value)
            {

                // clang-format off
                if (TER const ter = trustCreate(
                        view,
                        !dstHigh,                       // is dest low?
                        issuerAccID,                    // source
                        dstAccID,                       // destination
                        klDstLine.key,                  // ledger index
                        sleDstAcc,                      // Account to add to
                        false,                          // authorize account
                        (sleDstAcc->getFlags() & lsfDefaultRipple) == 0,
                        false,                          // freeze trust line
                        flipDstAmt ? -dstAmt : dstAmt,  // initial balance
                        Issue(currency, dstAccID),      // limit of zero
                        0,                              // quality in
                        0,                              // quality out
                        j);                             // journal
                    !isTesSuccess(ter))
                {
                    return ter;
                }
            }
        }
        // clang-format on
    }
    else
    {
        // the dst line does exist, and it would have been checked above
        // in trustTransferAllowed for NoRipple and Freeze flags

        // check the limit
        STAmount dstLimit =
            dstHigh ? (*sleDstLine)[sfHighLimit] : (*sleDstLine)[sfLowLimit];

        STAmount priorBalance =
            dstHigh ? -((*sleDstLine)[sfBalance]) : (*sleDstLine)[sfBalance];

        STAmount finalBalance = priorBalance + (flipDstAmt ? -dstAmt : dstAmt);

        if (finalBalance < priorBalance)
        {
            JLOG(j.warn())
                << "trustTransferLockedBalance resulted in a lower final balance on dest line";
            return tecINTERNAL;
        }

        if (finalBalance > dstLimit && actingAccID != dstAccID)
        {
            JLOG(j.trace())
                << "trustTransferLockedBalance would increase dest line above limit without permission";
            return tecPATH_DRY;
        }

        sleDstLine->setFieldAmount(sfBalance, dstHigh ? -finalBalance : finalBalance);
    }

    if (dryRun)
        return tesSUCCESS;

    static_assert(std::is_same<V, ApplyView>::value);

    // check if source line ended up in default state and adjust owner count if it did
    if (isTrustDefault(sleSrcAcc, sleSrcLine))
    {
        uint32_t flags = sleSrcLine->getFieldU32(sfFlags);
        uint32_t fReserve { srcHigh ? lsfHighReserve : lsfLowReserve };
        if (flags & fReserve)
        {
            sleSrcLine->setFieldU32(sfFlags, flags & ~fReserve);
            if (!dryRun)
            {
                adjustOwnerCount(view, sleSrcAcc, -1, j); 
                view.update(sleSrcAcc);
            }
        }
    }

    view.update(sleSrcLine);
    
    if (sleDstLine)
    {
        // a destination line already existed and was updated
        view.update(sleDstLine);
    }

    return tesSUCCESS;
}
/** Delete an offer.

    Requirements:
        The passed `sle` be obtained from a prior
        call to view.peek()
*/
// [[nodiscard]] // nodiscard commented out so Flow, BookTip and others compile.
TER
offerDelete(ApplyView& view, std::shared_ptr<SLE> const& sle, beast::Journal j);

//------------------------------------------------------------------------------

//
// Money Transfers
//

// Direct send w/o fees:
// - Redeeming IOUs and/or sending sender's own IOUs.
// - Create trust line of needed.
// --> bCheckIssuer : normally require issuer to be involved.
// [[nodiscard]] // nodiscard commented out so DirectStep.cpp compiles.
TER
rippleCredit(
    ApplyView& view,
    AccountID const& uSenderID,
    AccountID const& uReceiverID,
    const STAmount& saAmount,
    bool bCheckIssuer,
    beast::Journal j);

[[nodiscard]] TER
accountSend(
    ApplyView& view,
    AccountID const& from,
    AccountID const& to,
    const STAmount& saAmount,
    beast::Journal j);

[[nodiscard]] TER
issueIOU(
    ApplyView& view,
    AccountID const& account,
    STAmount const& amount,
    Issue const& issue,
    beast::Journal j);

[[nodiscard]] TER
redeemIOU(
    ApplyView& view,
    AccountID const& account,
    STAmount const& amount,
    Issue const& issue,
    beast::Journal j);

[[nodiscard]] TER
transferXRP(
    ApplyView& view,
    AccountID const& from,
    AccountID const& to,
    STAmount const& amount,
    beast::Journal j);

}  // namespace ripple

#endif
