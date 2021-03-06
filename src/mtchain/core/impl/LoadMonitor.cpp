//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/basics/Log.h>
#include <mtchain/basics/UptimeTimer.h>
#include <mtchain/beast/clock/chrono_util.h>
#include <mtchain/core/LoadMonitor.h>

namespace mtchain {

/*

TODO
----

- Use Journal for logging

*/

//------------------------------------------------------------------------------

LoadMonitor::Stats::Stats()
    : count (0)
    , latencyAvg (0)
    , latencyPeak (0)
    , isOverloaded (false)
{
}

//------------------------------------------------------------------------------

LoadMonitor::LoadMonitor (beast::Journal j)
    : mCounts (0)
    , mLatencyEvents (0)
    , mLatencyMSAvg (0)
    , mLatencyMSPeak (0)
    , mTargetLatencyAvg (0)
    , mTargetLatencyPk (0)
    , mLastUpdate (UptimeTimer::getInstance ().getElapsedSeconds ())
    , j_ (j)
{
}

// VFALCO NOTE WHY do we need "the mutex?" This dependence on
//         a hidden global, especially a synchronization primitive,
//         is a flawed design.
//         It's not clear exactly which data needs to be protected.
//
// call with the mutex
void LoadMonitor::update ()
{
    int now = UptimeTimer::getInstance ().getElapsedSeconds ();
    if (now == mLastUpdate) // current
        return;

    // VFALCO TODO Why 8?
    if ((now < mLastUpdate) || (now > (mLastUpdate + 8)))
    {
        // way out of date
        mCounts = 0;
        mLatencyEvents = 0;
        mLatencyMSAvg = 0;
        mLatencyMSPeak = 0;
        mLastUpdate = now;
        // VFALCO TODO don't return from the middle...
        return;
    }

    // do exponential decay
    /*
        David:

        "Imagine if you add 10 to something every second. And you
         also reduce it by 1/4 every second. It will "idle" at 40,
         correponding to 10 counts per second."
    */
    do
    {
        ++mLastUpdate;
        mCounts -= ((mCounts + 3) / 4);
        mLatencyEvents -= ((mLatencyEvents + 3) / 4);
        mLatencyMSAvg -= (mLatencyMSAvg / 4);
        mLatencyMSPeak -= (mLatencyMSPeak / 4);
    }
    while (mLastUpdate < now);
}

void LoadMonitor::addLoadSample (LoadEvent const& s)
{
    using namespace std::chrono;

    auto const total = s.runTime() + s.waitTime();
    // Don't include "jitter" as part of the latency
    auto const latency = total < 2ms ? 0ms : round<milliseconds>(total);

    if (latency > 500ms)
    {
        auto mj = (latency > 1s) ? j_.warn() : j_.info();
        JLOG (mj) << "Job: " << s.name() <<
            " run: " << round<milliseconds>(s.runTime()).count() << "ms" <<
            " wait: " << round<milliseconds>(s.waitTime()).count() << "ms";
    }

    addSamples (1, latency);
}

/* Add multiple samples
   @param count The number of samples to add
   @param latencyMS The total number of milliseconds
*/
void LoadMonitor::addSamples (int count, std::chrono::milliseconds latency)
{
    std::lock_guard<std::mutex> sl (mutex_);

    update ();
    mCounts += count;
    mLatencyEvents += count;
    mLatencyMSAvg += latency.count();
    mLatencyMSPeak += latency.count();

    int const latencyPeak = mLatencyEvents * latency.count() * 4 / count;

    if (mLatencyMSPeak < latencyPeak)
        mLatencyMSPeak = latencyPeak;
}

void LoadMonitor::setTargetLatency (std::uint64_t avg, std::uint64_t pk)
{
    mTargetLatencyAvg  = avg;
    mTargetLatencyPk = pk;
}

bool LoadMonitor::isOverTarget (std::uint64_t avg, std::uint64_t peak)
{
    return (mTargetLatencyPk && (peak > mTargetLatencyPk)) ||
           (mTargetLatencyAvg && (avg > mTargetLatencyAvg));
}

bool LoadMonitor::isOver ()
{
    std::lock_guard<std::mutex> sl (mutex_);

    update ();

    if (mLatencyEvents == 0)
        return 0;

    return isOverTarget (mLatencyMSAvg / (mLatencyEvents * 4), mLatencyMSPeak / (mLatencyEvents * 4));
}

LoadMonitor::Stats LoadMonitor::getStats ()
{
    Stats stats;

    std::lock_guard<std::mutex> sl (mutex_);

    update ();

    stats.count = mCounts / 4;

    if (mLatencyEvents == 0)
    {
        stats.latencyAvg = 0;
        stats.latencyPeak = 0;
    }
    else
    {
        stats.latencyAvg = mLatencyMSAvg / (mLatencyEvents * 4);
        stats.latencyPeak = mLatencyMSPeak / (mLatencyEvents * 4);
    }

    stats.isOverloaded = isOverTarget (stats.latencyAvg, stats.latencyPeak);

    return stats;
}

} //
