//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_PEERFINDER_SIM_FUNCTIONQUEUE_H_INCLUDED
#define MTCHAIN_PEERFINDER_SIM_FUNCTIONQUEUE_H_INCLUDED

namespace mtchain {
namespace PeerFinder {
namespace Sim {

/** Maintains a queue of functors that can be called later. */
class FunctionQueue
{
private:
    class BasicWork
    {
    public:
        virtual ~BasicWork ()
            { }
        virtual void operator() () = 0;
    };

    template <typename Function>
    class Work : public BasicWork
    {
    public:
        explicit Work (Function f)
            : m_f (f)
            { }
        void operator() ()
            { (m_f)(); }
    private:
        Function m_f;
    };

    std::list <std::unique_ptr <BasicWork>> m_work;

public:
    /** Returns `true` if there is no remaining work */
    bool empty ()
        { return m_work.empty(); }

    /** Queue a function.
        Function must be callable with this signature:
            void (void)
    */
    template <typename Function>
    void post (Function f)
    {
        m_work.emplace_back (std::make_unique<Work <Function>>(f));
    }

    /** Run all pending functions.
        The functions will be invoked in the order they were queued.
    */
    void run ()
    {
        while (! m_work.empty ())
        {
            (*m_work.front())();
            m_work.pop_front();
        }
    }
};

}
}
}

#endif
