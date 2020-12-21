//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_PROTOCOL_STPATHSET_H_INCLUDED
#define MTCHAIN_PROTOCOL_STPATHSET_H_INCLUDED

#include <mtchain/json/json_value.h>
#include <mtchain/protocol/SField.h>
#include <mtchain/protocol/STBase.h>
#include <mtchain/protocol/UintTypes.h>
#include <boost/optional.hpp>
#include <cassert>
#include <cstddef>

namespace mtchain {

class STPathElement
{
public:
    enum Type
    {
        typeNone        = 0x00,
        typeAccount     = 0x01, // Rippling through an account (vs taking an offer).
        typeCurrency    = 0x10, // Currency follows.
        typeIssuer      = 0x20, // Issuer follows.
        typeBoundary    = 0xFF, // Boundary between alternate paths.
        typeAll = typeAccount | typeCurrency | typeIssuer,
                                // Combination of all types.
    };

private:
    static
    std::size_t
    get_hash (STPathElement const& element);

public:
    STPathElement(
        boost::optional<AccountID> const& account,
        boost::optional<Currency> const& currency,
        boost::optional<AccountID> const& issuer)
        : mType (typeNone)
    {
        if (! account)
        {
            is_offer_ = true;
        }
        else
        {
            is_offer_ = false;
            mAccountID = *account;
            mType |= typeAccount;
            assert(mAccountID != noAccount());
        }

        if (currency)
        {
            mCurrencyID = *currency;
            mType |= typeCurrency;
        }

        if (issuer)
        {
            mIssuerID = *issuer;
            mType |= typeIssuer;
            assert(mIssuerID != noAccount());
        }

        hash_value_ = get_hash (*this);
    }

    STPathElement (
        AccountID const& account, Currency const& currency,
        AccountID const& issuer, bool forceCurrency = false)
        : mType (typeNone), mAccountID (account), mCurrencyID (currency)
        , mIssuerID (issuer), is_offer_ (isM(mAccountID))
    {
        if (!is_offer_)
            mType |= typeAccount;

        if (forceCurrency || !isM(currency))
            mType |= typeCurrency;

        if (!isM(issuer))
            mType |= typeIssuer;

        hash_value_ = get_hash (*this);
    }

    STPathElement (
        unsigned int uType, AccountID const& account, Currency const& currency,
        AccountID const& issuer)
        : mType (uType), mAccountID (account), mCurrencyID (currency)
        , mIssuerID (issuer), is_offer_ (isM(mAccountID))
    {
        hash_value_ = get_hash (*this);
    }

    STPathElement ()
        : mType (typeNone), is_offer_ (true)
    {
        hash_value_ = get_hash (*this);
    }

    int
    getNodeType () const
    {
        return mType;
    }

    bool
    isOffer () const
    {
        return is_offer_;
    }

    bool
    isAccount () const
    {
        return !isOffer ();
    }

    bool
    hasIssuer () const
    {
        return getNodeType () & STPathElement::typeIssuer;
    };
    bool
    hasCurrency () const
    {
        return getNodeType () & STPathElement::typeCurrency;
    };

    bool
    isNone () const
    {
        return getNodeType () == STPathElement::typeNone;
    };

    // Nodes are either an account ID or a offer prefix. Offer prefixs denote a
    // class of offers.
    AccountID const&
    getAccountID () const
    {
        return mAccountID;
    }

    Currency const&
    getCurrency () const
    {
        return mCurrencyID;
    }

    AccountID const&
    getIssuerID () const
    {
        return mIssuerID;
    }

    bool
    operator== (const STPathElement& t) const
    {
        return (mType & typeAccount) == (t.mType & typeAccount) &&
            hash_value_ == t.hash_value_ &&
            mAccountID == t.mAccountID &&
            mCurrencyID == t.mCurrencyID &&
            mIssuerID == t.mIssuerID;
    }

    bool
    operator!= (const STPathElement& t) const
    {
        return !operator==(t);
    }

private:
    unsigned int mType;
    AccountID mAccountID;
    Currency mCurrencyID;
    AccountID mIssuerID;

    bool is_offer_;
    std::size_t hash_value_;
};

class STPath
{
public:
    STPath () = default;

    STPath (std::vector<STPathElement> const& p)
        : mPath (p)
    { }

    std::vector<STPathElement>::size_type
    size () const
    {
        return mPath.size ();
    }

    bool empty() const
    {
        return mPath.empty ();
    }

    void
    push_back (STPathElement const& e)
    {
        mPath.push_back (e);
    }

    template <typename ...Args>
    void
    emplace_back (Args&&... args)
    {
        mPath.emplace_back (std::forward<Args> (args)...);
    }

    bool
    hasSeen (
        AccountID const& account, Currency const& currency,
        AccountID const& issuer) const;

    Json::Value
    getJson (int) const;

    std::vector<STPathElement>::const_iterator
    begin () const
    {
        return mPath.begin ();
    }

    std::vector<STPathElement>::const_iterator
    end () const
    {
        return mPath.end ();
    }

    bool
    operator== (STPath const& t) const
    {
        return mPath == t.mPath;
    }

    std::vector<STPathElement>::const_reference
    back () const
    {
        return mPath.back ();
    }

    std::vector<STPathElement>::const_reference
    front () const
    {
        return mPath.front ();
    }

    STPathElement& operator[](int i)
    {
        return mPath[i];
    }

    const STPathElement& operator[](int i) const
    {
        return mPath[i];
    }

private:
    std::vector<STPathElement> mPath;
};

//------------------------------------------------------------------------------

// A set of zero or more payment paths
class STPathSet final
    : public STBase
{
public:
    STPathSet () = default;

    STPathSet (SField const& n)
        : STBase (n)
    { }

    STPathSet (SerialIter& sit, SField const& name);

    STBase*
    copy (std::size_t n, void* buf) const override
    {
        return emplace(n, buf, *this);
    }

    STBase*
    move (std::size_t n, void* buf) override
    {
        return emplace(n, buf, std::move(*this));
    }

    void
    add (Serializer& s) const override;

    Json::Value
    getJson (int) const override;

    SerializedTypeID
    getSType () const override
    {
        return STI_PATHSET;
    }

    bool
    assembleAdd(STPath const& base, STPathElement const& tail);

    bool
    isEquivalent (const STBase& t) const override;

    bool
    isDefault () const override
    {
        return value.empty ();
    }

    // std::vector like interface:
    std::vector<STPath>::const_reference
    operator[] (std::vector<STPath>::size_type n) const
    {
        return value[n];
    }

    std::vector<STPath>::reference
    operator[] (std::vector<STPath>::size_type n)
    {
        return value[n];
    }

    std::vector<STPath>::const_iterator
    begin () const
    {
        return value.begin ();
    }

    std::vector<STPath>::const_iterator
    end () const
    {
        return value.end ();
    }

    std::vector<STPath>::size_type
    size () const
    {
        return value.size ();
    }

    bool
    empty () const
    {
        return value.empty ();
    }

    void
    push_back (STPath const& e)
    {
        value.push_back (e);
    }

    template <typename... Args>
    void emplace_back (Args&&... args)
    {
        value.emplace_back (std::forward<Args> (args)...);
    }

private:
    std::vector<STPath> value;
};

} //

#endif
