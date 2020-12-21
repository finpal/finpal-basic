//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/overlay/impl/TrafficCount.h>

namespace mtchain {

const char* TrafficCount::getName (category c)
{
    switch (c)
    {
        case category::CT_base:
            return "overhead";
        case category::CT_overlay:
            return "overlay";
        case category::CT_transaction:
            return "transactions";
        case category::CT_proposal:
            return "proposals";
        case category::CT_validation:
            return "validations";
        case category::CT_get_ledger:
            return "ledger_get";
        case category::CT_share_ledger:
            return "ledger_share";
        case category::CT_get_trans:
            return "transaction_set_get";
        case category::CT_share_trans:
            return "transaction_set_share";
        case category::CT_unknown:
            assert (false);
            return "unknown";
        default:
            assert (false);
            return "truly_unknow";
    }
}

TrafficCount::category TrafficCount::categorize (
    ::google::protobuf::Message const& message,
    int type, bool inbound)
{
    if ((type == protocol::mtHELLO) ||
            (type == protocol::mtPING) ||
            (type == protocol::mtCLUSTER) ||
            (type == protocol::mtSTATUS_CHANGE))
        return TrafficCount::category::CT_base;

    if ((type == protocol::mtMANIFESTS) ||
            (type == protocol::mtENDPOINTS) ||
            (type == protocol::mtPEERS) ||
            (type == protocol::mtGET_PEERS))
        return TrafficCount::category::CT_overlay;

    if (type == protocol::mtTRANSACTION)
        return TrafficCount::category::CT_transaction;

    if (type == protocol::mtVALIDATION)
        return TrafficCount::category::CT_validation;

    if (type == protocol::mtPROPOSE_LEDGER)
        return TrafficCount::category::CT_proposal;

    if (type == protocol::mtHAVE_SET)
        return inbound ? TrafficCount::category::CT_get_trans :
            TrafficCount::category::CT_share_trans;

    {
        auto msg = dynamic_cast
            <protocol::TMLedgerData const*> (&message);
        if (msg)
        {
            // We have received ledger data
            if (msg->type() == protocol::liTS_CANDIDATE)
                return (inbound && !msg->has_requestcookie()) ?
                    TrafficCount::category::CT_get_trans :
                    TrafficCount::category::CT_share_trans;
            return (inbound && !msg->has_requestcookie()) ?
                TrafficCount::category::CT_get_ledger :
                TrafficCount::category::CT_share_ledger;
        }
    }

    {
        auto msg =
            dynamic_cast <protocol::TMGetLedger const*>
                (&message);
        if (msg)
        {
            if (msg->itype() == protocol::liTS_CANDIDATE)
                return (inbound || msg->has_requestcookie()) ?
                    TrafficCount::category::CT_share_trans :
                    TrafficCount::category::CT_get_trans;
            return (inbound || msg->has_requestcookie()) ?
                TrafficCount::category::CT_share_ledger :
                TrafficCount::category::CT_get_ledger;
        }
    }

    {
        auto msg =
            dynamic_cast <protocol::TMGetObjectByHash const*>
                (&message);
        if (msg)
        {
            // inbound queries and outbound responses are sharing
            // outbound queries and inbound responses are getting
            return (msg->query() == inbound) ?
                TrafficCount::category::CT_share_ledger :
                TrafficCount::category::CT_get_ledger;
        }
    }

    assert (false);
    return TrafficCount::category::CT_unknown;
}

} //