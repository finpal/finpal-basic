//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_PEERFINDER_SLOT_H_INCLUDED
#define MTCHAIN_PEERFINDER_SLOT_H_INCLUDED

#include <mtchain/protocol/PublicKey.h>
#include <mtchain/beast/net/IPEndpoint.h>
#include <boost/optional.hpp>
#include <memory>

namespace mtchain {
namespace PeerFinder {

/** Properties and state associated with a peer to peer overlay connection. */
class Slot
{
public:
    using ptr = std::shared_ptr <Slot>;

    enum State
    {
        accept,
        connect,
        connected,
        active,
        closing
    };

    virtual ~Slot () = 0;

    /** Returns `true` if this is an inbound connection. */
    virtual bool inbound () const = 0;

    /** Returns `true` if this is a fixed connection.
        A connection is fixed if its remote endpoint is in the list of
        remote endpoints for fixed connections.
    */
    virtual bool fixed () const = 0;

    /** Returns `true` if this is a cluster connection.
        This is only known after then handshake completes.
    */
    virtual bool cluster () const = 0;

    /** Returns the state of the connection. */
    virtual State state () const = 0;

    /** The remote endpoint of socket. */
    virtual beast::IP::Endpoint const& remote_endpoint () const = 0;

    /** The local endpoint of the socket, when known. */
    virtual boost::optional <beast::IP::Endpoint> const& local_endpoint () const = 0;

    virtual boost::optional<std::uint16_t> listening_port () const = 0;

    /** The peer's public key, when known.
        The public key is established when the handshake is complete.
    */
    virtual boost::optional <PublicKey> const& public_key () const = 0;
};

}
}

#endif
