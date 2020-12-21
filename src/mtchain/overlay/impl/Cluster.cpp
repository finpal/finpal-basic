//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/app/main/Application.h>
#include <mtchain/basics/Log.h>
#include <mtchain/basics/StringUtilities.h>
#include <mtchain/core/Config.h>
#include <mtchain/core/TimeKeeper.h>
#include <mtchain/overlay/Cluster.h>
#include <mtchain/overlay/ClusterNode.h>
#include <mtchain/protocol/JsonFields.h>
#include <mtchain/protocol/tokens.h>
#include <boost/regex.hpp>
#include <memory.h>

namespace mtchain {

Cluster::Cluster (beast::Journal j)
    : j_ (j)
{
}

boost::optional<std::string>
Cluster::member (PublicKey const& identity) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto iter = nodes_.find (identity);
    if (iter == nodes_.end ())
        return boost::none;
    return iter->name ();
}

std::size_t
Cluster::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return nodes_.size();
}

bool
Cluster::update (
    PublicKey const& identity,
    std::string name,
    std::uint32_t loadFee,
    NetClock::time_point reportTime)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // We can't use auto here yet due to the libstdc++ issue
    // described at https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68190
    std::set<ClusterNode, Comparator>::iterator iter =
        nodes_.find (identity);

    if (iter != nodes_.end ())
    {
        if (reportTime <= iter->getReportTime())
            return false;

        if (name.empty())
            name = iter->name();

        iter = nodes_.erase (iter);
    }

    nodes_.emplace_hint (iter, identity, name, loadFee, reportTime);
    return true;
}

void
Cluster::for_each (
    std::function<void(ClusterNode const&)> func) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto const& ni : nodes_)
        func (ni);
}

bool
Cluster::load (Section const& nodes)
{
    static boost::regex const re (
        "[[:space:]]*"            // skip leading whitespace
        "([[:alnum:]]+)"          // node identity
        "(?:"                     // begin optional comment block
        "[[:space:]]+"            // (skip all leading whitespace)
        "(?:"                     // begin optional comment
        "(.*[^[:space:]]+)"       // the comment
        "[[:space:]]*"            // (skip all trailing whitespace)
        ")?"                      // end optional comment
        ")?"                      // end optional comment block
    );

    for (auto const& n : nodes.values())
    {
        boost::smatch match;

        if (!boost::regex_match (n, match, re))
        {
            JLOG (j_.error()) <<
                "Malformed entry: '" << n << "'";
            return false;
        }

        auto const id = parseBase58<PublicKey>(
            TokenType::TOKEN_NODE_PUBLIC, match[1]);

        if (!id)
        {
            JLOG (j_.error()) <<
                "Invalid node identity: " << match[1];
            return false;
        }

        if (member (*id))
        {
            JLOG (j_.warn()) <<
                "Duplicate node identity: " << match[1];
            continue;
        }

        update(*id, trim_whitespace(match[2]));
    }

    return true;
}

} //