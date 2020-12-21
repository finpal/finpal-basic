//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/peerfinder/impl/SlotImp.h>
#include <mtchain/peerfinder/PeerfinderManager.h>
#include <mtchain/peerfinder/impl/Tuning.h>

namespace mtchain {
namespace PeerFinder {

SlotImp::SlotImp (beast::IP::Endpoint const& local_endpoint,
    beast::IP::Endpoint const& remote_endpoint, bool fixed,
        clock_type& clock)
    : recent (clock)
    , m_inbound (true)
    , m_fixed (fixed)
    , m_cluster (false)
    , m_state (accept)
    , m_remote_endpoint (remote_endpoint)
    , m_local_endpoint (local_endpoint)
    , m_listening_port (unknownPort)
    , checked (false)
    , canAccept (false)
    , connectivityCheckInProgress (false)
{
}

SlotImp::SlotImp (beast::IP::Endpoint const& remote_endpoint,
    bool fixed, clock_type& clock)
    : recent (clock)
    , m_inbound (false)
    , m_fixed (fixed)
    , m_cluster (false)
    , m_state (connect)
    , m_remote_endpoint (remote_endpoint)
    , m_listening_port (unknownPort)
    , checked (true)
    , canAccept (true)
    , connectivityCheckInProgress (false)
{
}

void
SlotImp::state (State state_)
{
    // Must go through activate() to set active state
    assert (state_ != active);

    // The state must be different
    assert (state_ != m_state);

    // You can't transition into the initial states
    assert (state_ != accept && state_ != connect);

    // Can only become connected from outbound connect state
    assert (state_ != connected || (! m_inbound && m_state == connect));

    // Can't gracefully close on an outbound connection attempt
    assert (state_ != closing || m_state != connect);

    m_state = state_;
}

void
SlotImp::activate (clock_type::time_point const& now)
{
    // Can only become active from the accept or connected state
    assert (m_state == accept || m_state == connected);

    m_state = active;
    whenAcceptEndpoints = now;
}

//------------------------------------------------------------------------------

Slot::~Slot ()
{
}

//------------------------------------------------------------------------------

SlotImp::recent_t::recent_t (clock_type& clock)
    : cache (clock)
{
}

void
SlotImp::recent_t::insert (beast::IP::Endpoint const& ep, int hops)
{
    auto const result (cache.emplace (ep, hops));
    if (! result.second)
    {
        // NOTE Other logic depends on this <= inequality.
        if (hops <= result.first->second)
        {
            result.first->second = hops;
            cache.touch (result.first);
        }
    }
}

bool
SlotImp::recent_t::filter (beast::IP::Endpoint const& ep, int hops)
{
    auto const iter (cache.find (ep));
    if (iter == cache.end())
        return false;
    // We avoid sending an endpoint if we heard it
    // from them recently at the same or lower hop count.
    // NOTE Other logic depends on this <= inequality.
    return iter->second <= hops;
}

void
SlotImp::recent_t::expire ()
{
    beast::expire (cache,
        Tuning::liveCacheSecondsToLive);
}

}
}