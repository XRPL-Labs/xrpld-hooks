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
#include <functional>
#include <map>
#include <memory>
#include <utility>

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
        (std::is_same<V, ReadView const>::value && std::is_same<S, std::shared_ptr<SLE const>>::value) ||
        (std::is_same<V, ApplyView>::value && std::is_same<S, std::shared_ptr<SLE>>::value));

    // dry runs are explicit in code, but really the view type determines
    // what occurs here, so this combination is invalid.

    assert(!(std::is_same<V, ReadView const>::value && !dryRun));

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
        trustXferAllowed(
            view,
            parties,
            deltaAmt.issue());
        result != tesSUCCESS)
    {
        printf("trustXferAllowed failed on trustAdjustLockedBalance\n");
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


template<class V>
ReadView const&
forceReadView(V& view)
{
    static_assert(
        std::is_same<V, std::shared_ptr<ReadView const>>::value ||
        std::is_same<V, std::shared_ptr<ApplyView>>::value ||
        std::is_same<V, ApplyView>::value ||
        std::is_same<V, ReadView const>::value);

    ReadView const* rv = NULL;

    if constexpr (
        std::is_same<V, std::shared_ptr<ReadView const>>::value ||
        std::is_same<V, std::shared_ptr<ApplyView>>::value)
        rv = dynamic_cast<ReadView const*>(&(*view));
    else if constexpr(
        std::is_same<V, ApplyView>::value ||
        std::is_same<V, ReadView const>::value)
        rv = dynamic_cast<ReadView const*>(&view);
    
    return *rv;
}

// Check if movement of a particular token between 1 or more accounts
// (including unlocking) is forbidden by any flag or condition.
// If parties contains 1 entry then noRipple is not a bar to xfer.
// Part of featurePaychanAndEscrowForTokens, but can be callled without guard

template<class V>
[[nodiscard]]TER
trustXferAllowed(
    V& view_,
    std::vector<AccountID> const& parties,
    Issue const& issue)
{
    
    ReadView const& view = forceReadView(view_);

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
        auto line = view.read(keylet::line(p, issue.account, issue.currency));
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
            // but these are checked in trustXferLockedBalance at
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
                printf("trustXferAllowed: issuerFreeze\n");
                return tecFROZEN;
            }

            // if called with more than one party then any party
            // that has a noripple on the issuer side of their tl
            // blocks any possible xfer
            if (parties.size() > 1 && (flags & flagIssuerNoRipple))
            {
                printf("trustXferAllowed: issuerNoRipple\n");
                return tecPATH_DRY;
            }

            // every party involved must be on an authed trustline if
            // the issuer has specified lsfRequireAuth
            if (requireAuth && !(flags & flagIssuerAuth))
            {
                printf("trustXferAllowed: issuerRequireAuth\n");
                return tecNO_AUTH;
            }
        }
    }

    return tesSUCCESS;
}

[[nodiscard]] TER
trustXferLockedBalance(
    ApplyView& view,
    AccountID const& actingAccID, // the account whose tx is actioning xfer
    std::shared_ptr<SLE> const& sleSrcAcc,
    std::shared_ptr<SLE> const& sleDstAcc,
    STAmount const& amount,     // issuer, currency are in this field
    beast::Journal const& j);

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
