//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/app/tx/impl/OfferStream.h>
#include <mtchain/basics/Log.h>

namespace mtchain {

template<class TIn, class TOut>
TOfferStreamBase<TIn, TOut>::TOfferStreamBase (ApplyView& view, ApplyView& cancelView,
    Book const& book, NetClock::time_point when,
        StepCounter& counter, beast::Journal journal)
    : j_ (journal)
    , view_ (view)
    , cancelView_ (cancelView)
    , book_ (book)
    , expire_ (when)
    , tip_ (view, book_)
    , counter_ (counter)
{
}

// Handle the case where a directory item with no corresponding ledger entry
// is found. This shouldn't happen but if it does we clean it up.
template<class TIn, class TOut>
void
TOfferStreamBase<TIn, TOut>::erase (ApplyView& view)
{
    // NIKB NOTE This should be using ApplyView::dirDelete, which would
    //           correctly remove the directory if its the last entry.
    //           Unfortunately this is a protocol breaking change.

    auto p = view.peek (keylet::page(tip_.dir()));

    if (p == nullptr)
    {
        JLOG(j_.error()) <<
            "Missing directory " << tip_.dir() <<
            " for offer " << tip_.index();
        return;
    }

    auto v (p->getFieldV256 (sfIndexes));
    auto it (std::find (v.begin(), v.end(), tip_.index()));

    if (it == v.end())
    {
        JLOG(j_.error()) <<
            "Missing offer " << tip_.index() <<
            " for directory " << tip_.dir();
        return;
    }

    v.erase (it);
    p->setFieldV256 (sfIndexes, v);
    view.update (p);

    JLOG(j_.trace()) <<
        "Missing offer " << tip_.index() <<
        " removed from directory " << tip_.dir();
}

static
STAmount accountFundsHelper (ReadView const& view,
    AccountID const& id,
    STAmount const& saDefault,
    Issue const&,
    FreezeHandling freezeHandling,
    beast::Journal j)
{
    return accountFunds (view, id, saDefault, freezeHandling, j);
}

static
IOUAmount accountFundsHelper (ReadView const& view,
    AccountID const& id,
    IOUAmount const& amtDefault,
    Issue const& issue,
    FreezeHandling freezeHandling,
    beast::Journal j)
{
    if (issue.account == id)
        // self funded
        return amtDefault;

    return toAmount<IOUAmount> (
        accountHolds (view, id, issue.currency, issue.account, freezeHandling, j));
}

static
MAmount accountFundsHelper (ReadView const& view,
    AccountID const& id,
    MAmount const& amtDefault,
    Issue const& issue,
    FreezeHandling freezeHandling,
    beast::Journal j)
{
    return toAmount<MAmount> (
        accountHolds (view, id, issue.currency, issue.account, freezeHandling, j));
}

template<class TIn, class TOut>
bool
TOfferStreamBase<TIn, TOut>::step ()
{
    // Modifying the order or logic of these
    // operations causes a protocol breaking change.

    for(;;)
    {
        ownerFunds_ = boost::none;
        // BookTip::step deletes the current offer from the view before
        // advancing to the next (unless the ledger entry is missing).
        if (! tip_.step(j_))
            return false;

        std::shared_ptr<SLE> entry = tip_.entry();

        // If we exceed the maximum number of allowed steps, we're done.
        if (!counter_.step ())
            return false;

        // Remove if missing
        if (! entry)
        {
            erase (view_);
            erase (cancelView_);
            continue;
        }

        // Remove if expired
        using d = NetClock::duration;
        using tp = NetClock::time_point;
        if (entry->isFieldPresent (sfExpiration) &&
            tp{d{(*entry)[sfExpiration]}} <= expire_)
        {
            JLOG(j_.trace()) <<
                "Removing expired offer " << entry->key();
                permRmOffer (entry);
            continue;
        }

        offer_ = TOffer<TIn, TOut> (entry, tip_.quality());

        auto const amount (offer_.amount());

        // Remove if either amount is zero
        if (amount.empty())
        {
            JLOG(j_.warn()) <<
                "Removing bad offer " << entry->key();
            permRmOffer (entry);
            offer_ = TOffer<TIn, TOut>{};
            continue;
        }

        // Calculate owner funds
        ownerFunds_ = accountFundsHelper (view_, offer_.owner (), amount.out,
            offer_.issueOut (), fhZERO_IF_FROZEN, j_);

        // Check for unfunded offer
        if (*ownerFunds_ <= zero)
        {
            // If the owner's balance in the pristine view is the same,
            // we haven't modified the balance and therefore the
            // offer is "found unfunded" versus "became unfunded"
            auto const original_funds =
                accountFundsHelper (cancelView_, offer_.owner (), amount.out,
                    offer_.issueOut (), fhZERO_IF_FROZEN, j_);

            if (original_funds == *ownerFunds_)
            {
                permRmOffer (entry);
                JLOG(j_.trace()) <<
                    "Removing unfunded offer " << entry->key();
            }
            else
            {
                JLOG(j_.trace()) <<
                    "Removing became unfunded offer " << entry->key();
            }
            offer_ = TOffer<TIn, TOut>{};
            continue;
        }

        break;
    }

    return true;
}

void
OfferStream::permRmOffer (std::shared_ptr<SLE> const& sle)
{
    offerDelete (cancelView_,
                 cancelView_.peek(keylet::offer(sle->key())), j_);
}

template<class TIn, class TOut>
void FlowOfferStream<TIn, TOut>::permRmOffer (std::shared_ptr<SLE> const& sle)
{
    permToRemove_.insert (sle->key());
}

template class FlowOfferStream<STAmount, STAmount>;
template class FlowOfferStream<IOUAmount, IOUAmount>;
template class FlowOfferStream<MAmount, IOUAmount>;
template class FlowOfferStream<IOUAmount, MAmount>;

template class TOfferStreamBase<STAmount, STAmount>;
template class TOfferStreamBase<IOUAmount, IOUAmount>;
template class TOfferStreamBase<MAmount, IOUAmount>;
template class TOfferStreamBase<IOUAmount, MAmount>;
}
