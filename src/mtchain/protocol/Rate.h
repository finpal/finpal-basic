//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_PROTOCOL_RATE_H_INCLUDED
#define MTCHAIN_PROTOCOL_RATE_H_INCLUDED

#include <mtchain/protocol/STAmount.h>
#include <boost/operators.hpp>
#include <cassert>
#include <cstdint>
#include <ostream>

namespace mtchain {

/** Represents a transfer rate

    Transfer rates are specified as fractions of 1 billion.
    For example, a transfer rate of 1% is represented as
    1,010,000,000.
*/
struct Rate
    : private boost::totally_ordered <Rate>
{
    std::uint32_t value;

    Rate () = delete;

    explicit
    Rate (std::uint32_t rate)
        : value (rate)
    {
    }
};

inline
bool
operator== (Rate const& lhs, Rate const& rhs) noexcept
{
    return lhs.value == rhs.value;
}

inline
bool
operator< (Rate const& lhs, Rate const& rhs) noexcept
{
    return lhs.value < rhs.value;
}

inline
std::ostream&
operator<< (std::ostream& os, Rate const& rate)
{
    os << rate.value;
    return os;
}

STAmount
multiply (
    STAmount const& amount,
    Rate const& rate);

STAmount
multiplyRound (
    STAmount const& amount,
    Rate const& rate,
    bool roundUp);

STAmount
multiplyRound (
    STAmount const& amount,
    Rate const& rate,
    Issue const& issue,
    bool roundUp);

STAmount
divide (
    STAmount const& amount,
    Rate const& rate);

STAmount
divideRound (
    STAmount const& amount,
    Rate const& rate,
    bool roundUp);

STAmount
divideRound (
    STAmount const& amount,
    Rate const& rate,
    Issue const& issue,
    bool roundUp);

/** A transfer rate signifying a 1:1 exchange */
extern Rate const parityRate;

}

#endif
