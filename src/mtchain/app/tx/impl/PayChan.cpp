//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/app/tx/impl/PayChan.h>

#include <mtchain/basics/chrono.h>
#include <mtchain/basics/Log.h>
#include <mtchain/protocol/digest.h>
#include <mtchain/protocol/st.h>
#include <mtchain/protocol/Feature.h>
#include <mtchain/protocol/Indexes.h>
#include <mtchain/protocol/PayChan.h>
#include <mtchain/protocol/PublicKey.h>
#include <mtchain/protocol/TxFlags.h>
#include <mtchain/protocol/MAmount.h>
#include <mtchain/ledger/View.h>

namespace mtchain {

/*
    PaymentChannel

        Payment channels permit off-ledger checkpoints of M payments flowing
        in a single direction. A channel sequesters the owner's M in its own
        ledger entry. The owner can authorize the recipient to claim up to a
        given balance by giving the receiver a signed message (off-ledger). The
        recipient can use this signed message to claim any unpaid balance while
        the channel remains open. The owner can top off the line as needed. If
        the channel has not paid out all its funds, the owner must wait out a
        delay to close the channel to give the recipient a chance to supply any
        claims. The recipient can close the channel at any time. Any transaction
        that touches the channel after the expiration time will close the
        channel. The total amount paid increases monotonically as newer claims
        are issued. When the channel is closed any remaining balance is returned
        to the owner. Channels are intended to permit intermittent off-ledger
        settlement of ILP trust lines as balances get substantial. For
        bidirectional channels, a payment channel can be used in each direction.

    PaymentChannelCreate

        Create a unidirectional channel. The parameters are:
        Destination
            The recipient at the end of the channel.
        Amount
            The amount of M to deposit in the channel immediately.
        SettleDelay
            The amount of time everyone but the recipient must wait for a
            superior claim.
        PublicKey
            The key that will sign claims against the channel.
        CancelAfter (optional)
            Any channel transaction that touches this channel after the
            `CancelAfter` time will close it.
        DestinationTag (optional)
            Destination tags allow the different accounts inside of a Hosted
            Wallet to be mapped back onto the MTChain ledger. The destination tag
            tells the server to which account in the Hosted Wallet the funds are
            intended to go to. Required if the destination has lsfRequireDestTag
            set.
        SourceTag (optional)
            Source tags allow the different accounts inside of a Hosted Wallet
            to be mapped back onto the MTChain ledger. Source tags are similar to
            destination tags but are for the channel owner to identify their own
            transactions.

    PaymentChannelFund

        Add additional funds to the payment channel. Only the channel owner may
        use this transaction. The parameters are:
        Channel
            The 256-bit ID of the channel.
        Amount
            The amount of M to add.
        Expiration (optional)
            Time the channel closes. The transaction will fail if the expiration
            times does not satisfy the SettleDelay constraints.

    PaymentChannelClaim

        Place a claim against an existing channel. The parameters are:
        Channel
            The 256-bit ID of the channel.
        Balance (optional)
            The total amount of M delivered after this claim is processed (optional, not
            needed if just closing).
        Amount (optional)
            The amount of M the signature is for (not needed if equal to Balance or just
            closing the line).
        Signature (optional)
            Authorization for the balance above, signed by the owner (optional,
            not needed if closing or owner is performing the transaction). The
            signature if for the following message: CLM\0 followed by the
            256-bit channel ID, and a 64-bit integer drops.
        PublicKey (optional)
           The public key that made the signature (optional, required if a signature
           is present)
        Flags
            tfClose
                Request that the channel be closed
            tfRenew
                Request that the channel's expiration be reset. Only the owner
                may renew a channel.

*/

//------------------------------------------------------------------------------

static
TER
closeChannel (
    std::shared_ptr<SLE> const& slep,
    ApplyView& view,
    uint256 const& key,
    beast::Journal j)
{
    AccountID const src = (*slep)[sfAccount];
    // Remove PayChan from owner directory
    {
        auto const page = (*slep)[sfOwnerNode];
        TER const ter = dirDelete (view, true, page, keylet::ownerDir (src).key,
            key, false, page == 0, j);
        if (!isTesSuccess (ter))
            return ter;
    }

    // Transfer amount back to owner, decrement owner count
    auto const sle = view.peek (keylet::account (src));
    assert ((*slep)[sfAmount] >= (*slep)[sfBalance]);
    (*sle)[sfBalance] =
        (*sle)[sfBalance] + (*slep)[sfAmount] - (*slep)[sfBalance];
    (*sle)[sfOwnerCount] = (*sle)[sfOwnerCount] - 1;
    view.update (sle);

    // Remove PayChan from ledger
    view.erase (slep);
    return tesSUCCESS;
}

//------------------------------------------------------------------------------

TER
PayChanCreate::preflight (PreflightContext const& ctx)
{
    if (!ctx.rules.enabled (featurePayChan))
        return temDISABLED;

    auto const ret = preflight1 (ctx);
    if (!isTesSuccess (ret))
        return ret;

//    if (!isM (ctx.tx[sfAmount]) || (ctx.tx[sfAmount] <= beast::zero))
//        return temBAD_AMOUNT;

    if (ctx.tx[sfAccount] == ctx.tx[sfDestination])
        return temDST_IS_SRC;

    if (!publicKeyType(ctx.tx[sfPublicKey]))
        return temMALFORMED;

    return preflight2 (ctx);
}

TER
PayChanCreate::preclaim(PreclaimContext const &ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const sle = ctx.view.read (keylet::account (account));

    // Check reserve and funds availability
    {
        auto const balance = (*sle)[sfBalance];
        auto const reserve =
                ctx.view.fees ().accountReserve ((*sle)[sfOwnerCount] + 1);

        if (balance < reserve)
            return tecINSUFFICIENT_RESERVE;

        if (balance < reserve + ctx.tx[sfAmount])
            return tecUNFUNDED;
    }

    auto const dst = ctx.tx[sfDestination];

    {
        // Check destination account
        auto const sled = ctx.view.read (keylet::account (dst));
        if (!sled)
            return tecNO_DST;
        if (((*sled)[sfFlags] & lsfRequireDestTag) &&
            !ctx.tx[~sfDestinationTag])
            return tecDST_TAG_NEEDED;
        if ((*sled)[sfFlags] & lsfDisallowM)
            return tecNO_TARGET;
    }

    return tesSUCCESS;
}

TER
PayChanCreate::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const sle = ctx_.view ().peek (keylet::account (account));
    auto const dst = ctx_.tx[sfDestination];

    // Create PayChan in ledger
    auto const slep = std::make_shared<SLE> (
        keylet::payChan (account, dst, (*sle)[sfSequence] - 1));
    // Funds held in this channel
    (*slep)[sfAmount] = ctx_.tx[sfAmount];
    // Amount channel has already paid
    (*slep)[sfBalance] = ctx_.tx[sfAmount].zeroed ();
    (*slep)[sfAccount] = account;
    (*slep)[sfDestination] = dst;
    (*slep)[sfSettleDelay] = ctx_.tx[sfSettleDelay];
    (*slep)[sfPublicKey] = ctx_.tx[sfPublicKey];
    (*slep)[~sfCancelAfter] = ctx_.tx[~sfCancelAfter];
    (*slep)[~sfSourceTag] = ctx_.tx[~sfSourceTag];
    (*slep)[~sfDestinationTag] = ctx_.tx[~sfDestinationTag];

    ctx_.view ().insert (slep);

    // Add PayChan to owner directory
    {
        uint64_t page;
        auto result = dirAdd (ctx_.view (), page, keylet::ownerDir (account),
            slep->key (), describeOwnerDir (account),
            ctx_.app.journal ("View"));
        if (!isTesSuccess (result.first))
            return result.first;
        (*slep)[sfOwnerNode] = page;
    }

    // Deduct owner's balance, increment owner count
    (*sle)[sfBalance] = (*sle)[sfBalance] - ctx_.tx[sfAmount];
    (*sle)[sfOwnerCount] = (*sle)[sfOwnerCount] + 1;
    ctx_.view ().update (sle);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

TER
PayChanFund::preflight (PreflightContext const& ctx)
{
    if (!ctx.rules.enabled (featurePayChan))
        return temDISABLED;

    auto const ret = preflight1 (ctx);
    if (!isTesSuccess (ret))
        return ret;

    if (!isM (ctx.tx[sfAmount]) || (ctx.tx[sfAmount] <= beast::zero))
        return temBAD_AMOUNT;

    return preflight2 (ctx);
}

TER
PayChanFund::doApply()
{
    Keylet const k (ltPAYCHAN, ctx_.tx[sfPayChannel]);
    auto const slep = ctx_.view ().peek (k);
    if (!slep)
        return tecNO_ENTRY;

    AccountID const src = (*slep)[sfAccount];
    auto const txAccount = ctx_.tx[sfAccount];
    auto const expiration = (*slep)[~sfExpiration];

    {
        auto const cancelAfter = (*slep)[~sfCancelAfter];
        auto const closeTime =
            ctx_.view ().info ().parentCloseTime.time_since_epoch ().count ();
        if ((cancelAfter && closeTime >= *cancelAfter) ||
            (expiration && closeTime >= *expiration))
            return closeChannel (
                slep, ctx_.view (), k.key, ctx_.app.journal ("View"));
    }

    if (src != txAccount)
        // only the owner can add funds or extend
        return tecNO_PERMISSION;

    if (auto extend = ctx_.tx[~sfExpiration])
    {
        auto minExpiration =
            ctx_.view ().info ().parentCloseTime.time_since_epoch ().count () +
                (*slep)[sfSettleDelay];
        if (expiration && *expiration < minExpiration)
            minExpiration = *expiration;

        if (*extend < minExpiration)
            return temBAD_EXPIRATION;
        (*slep)[~sfExpiration] = *extend;
        ctx_.view ().update (slep);
    }

    auto const sle = ctx_.view ().peek (keylet::account (txAccount));

    {
        // Check reserve and funds availability
        auto const balance = (*sle)[sfBalance];
        auto const reserve =
            ctx_.view ().fees ().accountReserve ((*sle)[sfOwnerCount]);

        if (balance < reserve)
            return tecINSUFFICIENT_RESERVE;

        if (balance < reserve + ctx_.tx[sfAmount])
            return tecUNFUNDED;
    }

    (*slep)[sfAmount] = (*slep)[sfAmount] + ctx_.tx[sfAmount];
    ctx_.view ().update (slep);

    (*sle)[sfBalance] = (*sle)[sfBalance] - ctx_.tx[sfAmount];
    ctx_.view ().update (sle);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

TER
PayChanClaim::preflight (PreflightContext const& ctx)
{
    if (! ctx.rules.enabled(featurePayChan))
        return temDISABLED;

    auto const ret = preflight1 (ctx);
    if (!isTesSuccess (ret))
        return ret;

    auto const bal = ctx.tx[~sfBalance];
    if (bal && (!isM (*bal) || *bal <= beast::zero))
        return temBAD_AMOUNT;

    auto const amt = ctx.tx[~sfAmount];
    if (amt && (!isM (*amt) || *amt <= beast::zero))
        return temBAD_AMOUNT;

    if (bal && amt && *bal > *amt)
        return tecNO_PERMISSION;

    auto const flags = ctx.tx.getFlags ();
    if ((flags & tfClose) && (flags & tfRenew))
        return temMALFORMED;

    if (auto const sig = ctx.tx[~sfSignature])
    {
        if (!(ctx.tx[~sfPublicKey] && bal))
            return temMALFORMED;

        // Check the signature
        // The signature isn't needed if txAccount == src, but if it's
        // present, check it

        auto const reqBalance = bal->m ();
        auto const authAmt = amt ? amt->m() : reqBalance;

        if (reqBalance > authAmt)
            return tecNO_PERMISSION;

        Keylet const k (ltPAYCHAN, ctx.tx[sfPayChannel]);
        if (!publicKeyType(ctx.tx[sfPublicKey]))
            return temMALFORMED;
        PublicKey const pk (ctx.tx[sfPublicKey]);
        Serializer msg;
        serializePayChanAuthorization (msg, k.key, authAmt);
        if (!verify (pk, msg.slice (), *sig, /*canonical*/ true))
            return temBAD_SIGNATURE;
    }

    return preflight2 (ctx);
}

TER
PayChanClaim::doApply()
{
    Keylet const k (ltPAYCHAN, ctx_.tx[sfPayChannel]);
    auto const slep = ctx_.view ().peek (k);
    if (!slep)
        return tecNO_TARGET;

    AccountID const src = (*slep)[sfAccount];
    AccountID const dst = (*slep)[sfDestination];
    AccountID const txAccount = ctx_.tx[sfAccount];

    auto const curExpiration = (*slep)[~sfExpiration];
    {
        auto const cancelAfter = (*slep)[~sfCancelAfter];
        auto const closeTime =
            ctx_.view ().info ().parentCloseTime.time_since_epoch ().count ();
        if ((cancelAfter && closeTime >= *cancelAfter) ||
            (curExpiration && closeTime >= *curExpiration))
            return closeChannel (
                slep, ctx_.view (), k.key, ctx_.app.journal ("View"));
    }

    if (txAccount != src && txAccount != dst)
        return tecNO_PERMISSION;

    if (ctx_.tx[~sfBalance])
    {
        auto const chanBalance = slep->getFieldAmount (sfBalance).m ();
        auto const chanFunds = slep->getFieldAmount (sfAmount).m ();
        auto const reqBalance = ctx_.tx[sfBalance].m ();

        if (txAccount == dst && !ctx_.tx[~sfSignature])
            return temBAD_SIGNATURE;

        if (ctx_.tx[~sfSignature])
        {
            PublicKey const pk ((*slep)[sfPublicKey]);
            if (ctx_.tx[sfPublicKey] != pk)
                return temBAD_SIGNER;
        }

        if (reqBalance > chanFunds)
            return tecUNFUNDED_PAYMENT;

        if (reqBalance <= chanBalance)
            // nothing requested
            return tecUNFUNDED_PAYMENT;

        auto const sled = ctx_.view ().peek (keylet::account (dst));
        if (!sled)
            return terNO_ACCOUNT;

        if (txAccount == src && ((*sled)[sfFlags] & lsfDisallowM))
            return tecNO_TARGET;

        (*slep)[sfBalance] = ctx_.tx[sfBalance];
        MAmount const reqDelta = reqBalance - chanBalance;
        assert (reqDelta >= beast::zero);
        (*sled)[sfBalance] = (*sled)[sfBalance] + reqDelta;
        ctx_.view ().update (sled);
        ctx_.view ().update (slep);
    }

    if (ctx_.tx.getFlags () & tfRenew)
    {
        if (src != txAccount)
            return tecNO_PERMISSION;
        (*slep)[~sfExpiration] = boost::none;
        ctx_.view ().update (slep);
    }

    if (ctx_.tx.getFlags () & tfClose)
    {
        // Channel will close immediately if dry or the receiver closes
        if (dst == txAccount || (*slep)[sfBalance] == (*slep)[sfAmount])
            return closeChannel (
                slep, ctx_.view (), k.key, ctx_.app.journal ("View"));

        auto const settleExpiration =
            ctx_.view ().info ().parentCloseTime.time_since_epoch ().count () +
                (*slep)[sfSettleDelay];

        if (!curExpiration || *curExpiration > settleExpiration)
        {
            (*slep)[~sfExpiration] = settleExpiration;
            ctx_.view ().update (slep);
        }
    }

    return tesSUCCESS;
}

} //
