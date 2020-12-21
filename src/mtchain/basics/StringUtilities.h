//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_BASICS_STRINGUTILITIES_H_INCLUDED
#define MTCHAIN_BASICS_STRINGUTILITIES_H_INCLUDED

#include <mtchain/basics/ByteOrder.h>
#include <mtchain/basics/Blob.h>
#include <mtchain/basics/strHex.h>
#include <boost/format.hpp>
#include <sstream>
#include <string>

namespace mtchain {

// NIKB TODO Remove the need for all these overloads. Move them out of here.
inline const std::string strHex (std::string const& strSrc)
{
    return strHex (strSrc.begin (), strSrc.size ());
}

inline std::string strHex (Blob const& vucData)
{
    return strHex (vucData.begin (), vucData.size ());
}

inline std::string strHex (const std::uint64_t uiHost)
{
    uint64_t    uBig    = htobe64 (uiHost);

    return strHex ((unsigned char*) &uBig, sizeof (uBig));
}

inline static std::string sqlEscape (std::string const& strSrc)
{
    static boost::format f ("X'%s'");
    return str (boost::format (f) % strHex (strSrc));
}

inline static std::string sqlEscape (Blob const& vecSrc)
{
    size_t size = vecSrc.size ();

    if (size == 0)
        return "X''";

    std::string j (size * 2 + 3, 0);

    unsigned char* oPtr = reinterpret_cast<unsigned char*> (&*j.begin ());
    const unsigned char* iPtr = &vecSrc[0];

    *oPtr++ = 'X';
    *oPtr++ = '\'';

    for (int i = size; i != 0; --i)
    {
        unsigned char c = *iPtr++;
        *oPtr++ = charHex (c >> 4);
        *oPtr++ = charHex (c & 15);
    }

    *oPtr++ = '\'';
    return j;
}

uint64_t uintFromHex (std::string const& strSrc);

std::pair<Blob, bool> strUnHex (std::string const& strSrc);

struct parsedURL
{
    std::string scheme;
    std::string domain;
    boost::optional<std::uint16_t> port;
    std::string path;
};

bool parseUrl (parsedURL& pUrl, std::string const& strUrl);

std::string trim_whitespace (std::string str);

bool isHexStr (std::string const& txid, std::uint32_t size);
static inline bool isHex64(std::string const& txid)
{
    return isHexStr(txid, 64);
}

std::string to_string(Blob const& b);
Blob to_blob(const char* s);
static inline Blob to_blob(std::string const& str)
{
    return to_blob(str.c_str());
}
std::tuple<std::uint64_t, std::uint64_t> parse_fraction(std::string const& s);

} //

#endif
