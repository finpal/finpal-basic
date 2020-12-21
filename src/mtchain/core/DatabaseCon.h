//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_APP_DATA_DATABASECON_H_INCLUDED
#define MTCHAIN_APP_DATA_DATABASECON_H_INCLUDED

#include <mtchain/core/Config.h>
#include <mtchain/core/SociDB.h>
#include <boost/filesystem/path.hpp>
#include <mutex>
#include <string>


namespace soci {
    class session;
}

namespace mtchain {

template<class T, class TMutex>
class LockedPointer
{
public:
    using mutex = TMutex;
private:
    T* it_;
    std::unique_lock<mutex> lock_;

public:
    LockedPointer (T* it, mutex& m) : it_ (it), lock_ (m)
    {
    }
    LockedPointer (LockedPointer&& rhs) noexcept
        : it_ (rhs.it_), lock_ (std::move (rhs.lock_))
    {
    }
    LockedPointer () = delete;
    LockedPointer (LockedPointer const& rhs) = delete;
    LockedPointer& operator=(LockedPointer const& rhs) = delete;

    T* get ()
    {
        return it_;
    }
    T& operator*()
    {
        return *it_;
    }
    T* operator->()
    {
        return it_;
    }
    explicit operator bool() const
    {
        return bool (it_);
    }
};

using LockedSociSession = LockedPointer<soci::session, std::recursive_mutex>;

class DatabaseCon
{
public:
    struct Setup
    {
        Config::StartUpType startUp = Config::NORMAL;
        bool standAlone = false;
        boost::filesystem::path dataDir;
    };

    DatabaseCon (Setup const& setup,
                 std::string const& name,
                 const char* initString[],
                 int countInit);

    soci::session& getSession()
    {
        return session_;
    }

    LockedSociSession checkoutDb ()
    {
        return LockedSociSession (&session_, lock_);
    }

    void setupCheckpointing (JobQueue*, Logs&);

private:
    LockedSociSession::mutex lock_;

    soci::session session_;
    std::unique_ptr<Checkpointer> checkpointer_;
};

DatabaseCon::Setup
setup_DatabaseCon (Config const& c);

} //

#endif
