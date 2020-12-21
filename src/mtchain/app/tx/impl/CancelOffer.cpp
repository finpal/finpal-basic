//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/app/tx/impl/CancelOffer.h>
#include <mtchain/basics/Log.h>
#include <mtchain/protocol/st.h>
#include <mtchain/ledger/View.h>

namespace mtchain {

TER
CancelOffer::preflight (PreflightContext const& ctx)
{
    auto const ret = preflight1 (ctx);
    if (!isTesSuccess (ret))
        return ret;

    auto const uTxFlags = ctx.tx.getFlags();

    if (uTxFlags & tfUniversalMask)
    {
        JLOG(ctx.j.trace()) << "Malformed transaction: " <<
            "Invalid flags set.";
        return temINVALID_FLAG;
    }

    auto const seq = ctx.tx.getFieldU32 (sfOfferSequence);
    if (! seq)
    {
        JLOG(ctx.j.trace()) <<
            "CancelOffer::preflight: missing sequence";
        return temBAD_SEQUENCE;
    }

    return preflight2(ctx);
}

//------------------------------------------------------------------------------

TER
CancelOffer::preclaim(PreclaimContext const& ctx)
{
    auto const id = ctx.tx[sfAccount];
    auto const offerSequence = ctx.tx[sfOfferSequence];

    auto const sle = ctx.view.read(
        keylet::account(id));

    if ((*sle)[sfSequence] <= offerSequence)
    {
        JLOG(ctx.j.trace()) << "Malformed transaction: " <<
            "Sequence " << offerSequence << " is invalid.";
        return temBAD_SEQUENCE;
    }

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

TER
CancelOffer::doApply ()
{
    auto const offerSequence = ctx_.tx[sfOfferSequence];

    auto const sle = view().read(
        keylet::account(account_));

    uint256 const offerIndex (getOfferIndex (account_, offerSequence));

    auto sleOffer = view().peek (
        keylet::offer(offerIndex));

    if (sleOffer)
    {
        JLOG(j_.debug()) << "Trying to cancel offer #" << offerSequence;
        return offerDelete (view(), sleOffer, ctx_.app.journal("View"));
    }

    JLOG(j_.debug()) << "Offer #" << offerSequence << " can't be found.";
    return tesSUCCESS;
}

}
