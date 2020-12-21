//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_CORE_DEADLINETIMER_H_INCLUDED
#define MTCHAIN_CORE_DEADLINETIMER_H_INCLUDED

#include <mtchain/beast/core/List.h>
#include <chrono>

namespace mtchain {

/** Provides periodic or one time notifications at a specified time interval.
*/
class DeadlineTimer
    : public beast::List <DeadlineTimer>::Node
{
public:
    using clock = std::chrono::steady_clock;     ///< DeadlineTimer clock.
    using duration = std::chrono::milliseconds;  ///< DeadlineTimer duration.
    /** DeadlineTimer time_point. */
    using time_point = std::chrono::time_point<clock, duration>;

    /** Listener for a deadline timer.

        The listener is called on an auxiliary thread. It is suggested
        not to perform any time consuming operations during the call.
    */
    // VFALCO TODO Perhaps allow construction using a ServiceQueue to use
    //             for notifications.
    //
    class Listener
    {
    public:
        /** Entry point called by DeadlineTimer when a deadline elapses. */
        virtual void onDeadlineTimer (DeadlineTimer&) = 0;
    };

    /** Create a deadline timer with the specified listener attached.

        @param listener pointer to Listener that is called at the deadline.
    */
    explicit DeadlineTimer (Listener* listener);

    /// @cond INTERNAL
    DeadlineTimer (DeadlineTimer const&) = delete;
    DeadlineTimer& operator= (DeadlineTimer const&) = delete;
    /// @endcond

    /** Destructor. */
    ~DeadlineTimer ();

    /** Cancel all notifications.
        It is okay to call this on an inactive timer.
        @note It is guaranteed that no notifications will occur after this
              function returns.
    */
    void cancel ();

    /** Set the timer to go off once in the future.
        If the timer is already active, this will reset it.
        @note If the timer is already active, the old one might go off
              before this function returns.
        @param delay duration until the timer will send a notification.
                     This must be greater than zero.
    */
    void setExpiration (duration delay);

    /** Set the timer to go off repeatedly with the specified period.
        If the timer is already active, this will reset it.
        @note If the timer is already active, the old one might go off
              before this function returns.
        @param interval duration until the timer will send a notification.
                        This must be greater than zero.
    */
    void setRecurringExpiration (duration interval);

    /** Equality comparison.
        Timers are equal if they have the same address.
    */
    inline bool operator== (DeadlineTimer const& other) const
    {
        return this == &other;
    }

    /** Inequality comparison. */
    inline bool operator!= (DeadlineTimer const& other) const
    {
        return this != &other;
    }

private:
    class Manager;

    Listener* const m_listener;
    bool m_isActive;

    time_point notificationTime_;
    duration recurring_; // > 0ms if recurring.
};

}

#endif
