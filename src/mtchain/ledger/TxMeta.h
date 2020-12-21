//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_APP_TX_TRANSACTIONMETA_H_INCLUDED
#define MTCHAIN_APP_TX_TRANSACTIONMETA_H_INCLUDED

#include <mtchain/protocol/STLedgerEntry.h>
#include <mtchain/protocol/STArray.h>
#include <mtchain/protocol/TER.h>
#include <mtchain/beast/utility/Journal.h>
#include <boost/container/flat_set.hpp>
#include <boost/optional.hpp>

namespace mtchain {

// VFALCO Move to mtchain/app/ledger/detail, rename to TxMeta
class TxMeta
{
public:
    using pointer = std::shared_ptr<TxMeta>;
    using ref = const pointer&;

private:
    struct CtorHelper{};
    template<class T>
    TxMeta (uint256 const& txID, std::uint32_t ledger, T const& data, beast::Journal j,
                        CtorHelper);
public:
    TxMeta (beast::Journal j)
        : mLedger (0)
        , mIndex (static_cast<std::uint32_t> (-1))
        , mResult (255)
        , j_ (j)
    {
    }

    TxMeta (uint256 const& txID, std::uint32_t ledger, std::uint32_t index, beast::Journal j)
        : mTransactionID (txID)
        , mLedger (ledger)
        , mIndex (static_cast<std::uint32_t> (-1))
        , mResult (255)
        , j_(j)
    {
    }

    TxMeta (uint256 const& txID, std::uint32_t ledger, Blob const&, beast::Journal j);
    TxMeta (uint256 const& txID, std::uint32_t ledger, std::string const&, beast::Journal j);
    TxMeta (uint256 const& txID, std::uint32_t ledger, STObject const&, beast::Journal j);

    void init (uint256 const& transactionID, std::uint32_t ledger);
    void clear ()
    {
        mNodes.clear ();
    }
    void swap (TxMeta&) noexcept;

    uint256 const& getTxID ()
    {
        return mTransactionID;
    }
    std::uint32_t getLgrSeq ()
    {
        return mLedger;
    }
    int getResult () const
    {
        return mResult;
    }
    TER getResultTER () const
    {
        return static_cast<TER> (mResult);
    }
    std::uint32_t getIndex () const
    {
        return mIndex;
    }

    bool isNodeAffected (uint256 const& ) const;
    void setAffectedNode (uint256 const& , SField const& type,
                          std::uint16_t nodeType);
    STObject& getAffectedNode (SLE::ref node, SField const& type); // create if needed
    STObject& getAffectedNode (uint256 const& );
    const STObject& peekAffectedNode (uint256 const& ) const;

    /** Return a list of accounts affected by this transaction */
    boost::container::flat_set<AccountID>
    getAffectedAccounts() const;

    Json::Value getJson (int p) const
    {
        return getAsObject ().getJson (p);
    }
    void addRaw (Serializer&, TER, std::uint32_t index);

    STObject getAsObject () const;
    STArray& getNodes ()
    {
        return (mNodes);
    }

    void setDeliveredAmount (STAmount const& delivered)
    {
        mDelivered.reset (delivered);
    }

    STAmount getDeliveredAmount () const
    {
        assert (hasDeliveredAmount ());
        return *mDelivered;
    }

    bool hasDeliveredAmount () const
    {
        return static_cast <bool> (mDelivered);
    }

    void setMetaExtra (STObject && extra)
    {
        mExtra = extra;
    }

    STObject& getMetaExtra ()
    {
        return mExtra;
    }

    static bool thread (STObject& node, uint256 const& prevTxID, std::uint32_t prevLgrID);

private:
    uint256       mTransactionID;
    std::uint32_t mLedger;
    std::uint32_t mIndex;
    int           mResult;

    boost::optional <STAmount> mDelivered;
    STObject mExtra = STObject(sfMetadata);

    STArray mNodes;

    beast::Journal j_;
};

} //

#endif