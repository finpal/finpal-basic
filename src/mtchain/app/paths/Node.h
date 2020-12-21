//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_APP_PATHS_NODE_H_INCLUDED
#define MTCHAIN_APP_PATHS_NODE_H_INCLUDED

#include <mtchain/app/paths/NodeDirectory.h>
#include <mtchain/app/paths/Types.h>
#include <mtchain/protocol/Rate.h>
#include <mtchain/protocol/UintTypes.h>
#include <boost/optional.hpp>

namespace mtchain {
namespace path {

struct Node
{
    using List = std::vector<Node>;

    inline bool isAccount() const
    {
        return (uFlags & STPathElement::typeAccount);
    }

    Json::Value getJson () const;

    bool operator == (Node const&) const;

    std::uint16_t uFlags;       // --> From path.

    AccountID account_;         // --> Accounts: Receiving/sending account.

    Issue issue_;               // --> Accounts: Receive and send, Offers: send.
                                // --- For offer's next has currency out.

    boost::optional<Rate> transferRate_;         // Transfer rate for issuer.

    // Computed by Reverse.
    STAmount saRevRedeem;        // <-- Amount to redeem to next.
    STAmount saRevIssue;         // <-- Amount to issue to next, limited by
                                 //     credit and outstanding IOUs.  Issue
                                 //     isn't used by offers.
    STAmount saRevDeliver;       // <-- Amount to deliver to next regardless of
                                 // fee.

    // Computed by forward.
    STAmount saFwdRedeem;        // <-- Amount node will redeem to next.
    STAmount saFwdIssue;         // <-- Amount node will issue to next.
                                 //     Issue isn't used by offers.
    STAmount saFwdDeliver;       // <-- Amount to deliver to next regardless of
                                 // fee.

    // For offers:
    boost::optional<Rate> rateMax;

    // The nodes are partitioned into a buckets called "directories".
    //
    // Each "directory" contains nodes with exactly the same "quality" (meaning
    // the conversion rate between one corrency and the next).
    //
    // The "directories" are ordered in "increasing" "quality" value, which
    // means that the first "directory" has the "best" (i.e. numerically least)
    // "quality".
    // https://mtchain.io/wiki/Ledger_Format#Prioritizing_a_continuous_key_space

    NodeDirectory directory;

    STAmount saOfrRate;          // For correct ratio.

    // PaymentNode
    bool bEntryAdvance;          // Need to advance entry.
    unsigned int uEntry;
    uint256 offerIndex_;
    SLE::pointer sleOffer;
    AccountID offerOwnerAccount_;

    // Do we need to refresh saOfferFunds, saTakerPays, & saTakerGets?
    bool bFundsDirty;
    STAmount saOfferFunds;
    STAmount saTakerPays;
    STAmount saTakerGets;

    /** Clear input and output amounts. */
    void clear()
    {
        saRevRedeem.clear ();
        saRevIssue.clear ();
        saRevDeliver.clear ();
        saFwdDeliver.clear ();
    }
};

} // path
} //

#endif