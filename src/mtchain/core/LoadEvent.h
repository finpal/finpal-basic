//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_CORE_LOADEVENT_H_INCLUDED
#define MTCHAIN_CORE_LOADEVENT_H_INCLUDED

#include <chrono>
#include <memory>
#include <string>

namespace mtchain {

class LoadMonitor;

// VFALCO NOTE What is the difference between a LoadEvent and a LoadMonitor?
// VFALCO TODO Rename LoadEvent to ScopedLoadSample
//
//        This looks like a scoped elapsed time measuring class
//
class LoadEvent
{
public:
    // VFALCO TODO remove the dependency on LoadMonitor. Is that possible?
    LoadEvent (LoadMonitor& monitor,
               std::string const& name,
               bool shouldStart);
    LoadEvent(LoadEvent const&) = delete;

    ~LoadEvent ();

    std::string const&
    name () const;

    // The time spent waiting.
    std::chrono::steady_clock::duration
    waitTime() const;

    // The time spent running.
    std::chrono::steady_clock::duration
    runTime() const;

    // VFALCO TODO rename this to setName () or setLabel ()
    void reName (std::string const& name);

    // Start the measurement. If already started, then
    // restart, assigning the elapsed time to the "waiting"
    // state.
    void start ();

    // Stop the measurement and report the results. The
    // time reported is measured from the last call to
    // start.
    void stop ();

private:
    LoadMonitor& monitor_;

    // Represents our current state
    bool running_;

    // The name associated with this event, if any.
    std::string name_;

    // Represents the time we last transitioned states
    std::chrono::steady_clock::time_point mark_;

    // The time we spent waiting and running respectively
    std::chrono::steady_clock::duration timeWaiting_;
    std::chrono::steady_clock::duration timeRunning_;
};

} //

#endif
