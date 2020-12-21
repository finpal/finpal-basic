//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_APP_BASICAPP_H_INCLUDED
#define MTCHAIN_APP_BASICAPP_H_INCLUDED

#include <boost/asio/io_service.hpp>
#include <boost/optional.hpp>
#include <thread>
#include <vector>

// This is so that the io_service can outlive all the children
class BasicApp
{
private:
    boost::optional<boost::asio::io_service::work> work_;
    std::vector<std::thread> threads_;
    boost::asio::io_service io_service_;

protected:
    BasicApp(std::size_t numberOfThreads);
    ~BasicApp();

public:
    boost::asio::io_service&
    get_io_service()
    {
        return io_service_;
    }
};

#endif
