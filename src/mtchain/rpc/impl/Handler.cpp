//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/rpc/impl/Handler.h>
#include <mtchain/rpc/handlers/Handlers.h>
#include <mtchain/rpc/handlers/Version.h>

namespace mtchain {
namespace RPC {
namespace {

/** Adjust an old-style handler to be call-by-reference. */
template <typename Function>
Handler::Method<Json::Value> byRef (Function const& f)
{
    return [f] (Context& context, Json::Value& result)
    {
        result = f (context);
        if (result.type() != Json::objectValue)
        {
            assert (false);
            result = RPC::makeObjectValue (result);
        }

        return Status();
    };
}

template <class Object, class HandlerImpl>
Status handle (Context& context, Object& object)
{
    HandlerImpl handler (context);

    auto status = handler.check ();
    if (status)
        status.inject (object);
    else
        handler.writeResult (object);
    return status;
};

class HandlerTable {
  public:
    template<std::size_t N>
    explicit
    HandlerTable (const Handler(&entries)[N])
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            auto const& entry = entries[i];
            assert (table_.find(entry.name_) == table_.end());
            table_[entry.name_] = entry;
        }

        // This is where the new-style handlers are added.
        addHandler<LedgerHandler>();
        addHandler<VersionHandler>();
    }

    const Handler* getHandler(std::string name) const {
        auto i = table_.find(name);
        return i == table_.end() ? nullptr : &i->second;
    }

  private:
    std::map<std::string, Handler> table_;

    template <class HandlerImpl>
    void addHandler()
    {
        assert (table_.find(HandlerImpl::name()) == table_.end());

        Handler h;
        h.name_ = HandlerImpl::name();
        h.valueMethod_ = &handle<Json::Value, HandlerImpl>;
        h.role_ = HandlerImpl::role();
        h.condition_ = HandlerImpl::condition();
        h.objectMethod_ = &handle<Json::Object, HandlerImpl>;

        table_[HandlerImpl::name()] = h;
    };
};

Handler handlerArray[] {
    // Some handlers not specified here are added to the table via addHandler()
    // Request-response methods
    {   "account_info",         byRef (&doAccountInfo),       Role::USER,  NO_CONDITION  },
    {   "account_currencies",   byRef (&doAccountCurrencies), Role::USER,  NO_CONDITION  },
    {   "account_lines",        byRef (&doAccountLines),      Role::USER,  NO_CONDITION  },
    {   "account_channels",     byRef (&doAccountChannels),   Role::USER,  NO_CONDITION  },
    {   "account_objects",      byRef (&doAccountObjects),    Role::USER,  NO_CONDITION  },
    {   "account_offers",       byRef (&doAccountOffers),     Role::USER,  NO_CONDITION  },
    {   "account_tx",           byRef (&doAccountTxSwitch),   Role::USER,  NO_CONDITION  },
    {   "blacklist",            byRef (&doBlackList),         Role::ADMIN, NO_CONDITION  },
    {   "book_offers",          byRef (&doBookOffers),        Role::USER,  NO_CONDITION  },
    {   "can_delete",           byRef (&doCanDelete),         Role::ADMIN, NO_CONDITION  },
    {   "channel_authorize",    byRef (&doChannelAuthorize),  Role::USER,  NO_CONDITION  },
    {   "channel_verify",       byRef (&doChannelVerify),     Role::USER,  NO_CONDITION  },
    {   "connect",              byRef (&doConnect),           Role::ADMIN, NO_CONDITION  },
    {   "consensus_info",       byRef (&doConsensusInfo),     Role::ADMIN, NO_CONDITION  },
    {   "gateway_balances",     byRef (&doGatewayBalances),   Role::USER,  NO_CONDITION  },
    {   "get_counts",           byRef (&doGetCounts),         Role::ADMIN, NO_CONDITION  },
    {   "feature",              byRef (&doFeature),           Role::ADMIN, NO_CONDITION  },
    {   "fee",                  byRef (&doFee),               Role::USER,  NO_CONDITION  },
    {   "fetch_info",           byRef (&doFetchInfo),         Role::ADMIN, NO_CONDITION  },
    {   "ipfs_fee_info",        byRef (&doIpfsFeeInfo),       Role::USER,  NO_CONDITION  },
    {   "ledger_accept",        byRef (&doLedgerAccept),      Role::ADMIN, NEEDS_CURRENT_LEDGER },
    {   "ledger_cleaner",       byRef (&doLedgerCleaner),     Role::ADMIN, NEEDS_NETWORK_CONNECTION  },
    {   "ledger_closed",        byRef (&doLedgerClosed),      Role::USER,  NO_CONDITION   },
    {   "ledger_current",       byRef (&doLedgerCurrent),     Role::USER,  NEEDS_CURRENT_LEDGER  },
    {   "ledger_data",          byRef (&doLedgerData),        Role::USER,  NO_CONDITION  },
    {   "ledger_entry",         byRef (&doLedgerEntry),       Role::USER,  NO_CONDITION  },
    {   "ledger_entry_index",   byRef (&doLedgerEntryIndex),  Role::USER,  NO_CONDITION  },
    {   "ledger_header",        byRef (&doLedgerHeader),      Role::USER,  NO_CONDITION  },
    {   "ledger_request",       byRef (&doLedgerRequest),     Role::USER,  NO_CONDITION  },
    {   "log_level",            byRef (&doLogLevel),          Role::ADMIN, NO_CONDITION  },
    {   "logrotate",            byRef (&doLogRotate),         Role::ADMIN, NO_CONDITION  },
    {   "noFinPal_check",       byRef (&doNoMTChainCheck),    Role::USER,  NO_CONDITION  },
    {   "owner_info",           byRef (&doOwnerInfo),         Role::USER,  NEEDS_CURRENT_LEDGER  },
    {   "peers",                byRef (&doPeers),             Role::USER,  NO_CONDITION  },
    {   "path_find",            byRef (&doPathFind),          Role::USER,  NEEDS_CURRENT_LEDGER  },
    {   "ping",                 byRef (&doPing),              Role::USER,  NO_CONDITION  },
    {   "print",                byRef (&doPrint),             Role::ADMIN, NO_CONDITION  },
//  {   "profile",              byRef (&doProfile),           Role::USER,  NEEDS_CURRENT_LEDGER  },
    {   "random",               byRef (&doRandom),            Role::USER,  NO_CONDITION  },
    {   "jss::FinPal_path_find",     byRef (&doMTChainPathFind),    Role::USER,  NO_CONDITION  },
    {   "sign",                 byRef (&doSign),              Role::USER,  NO_CONDITION  },
    {   "sign_for",             byRef (&doSignFor),           Role::USER,  NO_CONDITION  },
    {   "submit",               byRef (&doSubmit),            Role::USER,  NEEDS_CURRENT_LEDGER  },
    {   "submit_multisigned",   byRef (&doSubmitMultiSigned), Role::USER,  NEEDS_CURRENT_LEDGER  },
    {   "server_info",          byRef (&doServerInfo),        Role::USER,  NO_CONDITION  },
    {   "server_state",         byRef (&doServerState),       Role::USER,  NO_CONDITION  },
    {   "stop",                 byRef (&doStop),              Role::ADMIN, NO_CONDITION  },
    {   "transaction_entry",    byRef (&doTransactionEntry),  Role::USER,  NO_CONDITION  },
    {   "tx",                   byRef (&doTx),                Role::USER,  NEEDS_NETWORK_CONNECTION  },
    {   "tx_history",           byRef (&doTxHistory),         Role::USER,  NO_CONDITION  },
    {   "unl_list",             byRef (&doUnlList),           Role::ADMIN, NO_CONDITION  },
    {   "validation_create",    byRef (&doValidationCreate),  Role::ADMIN, NO_CONDITION  },
    {   "validation_seed",      byRef (&doValidationSeed),    Role::ADMIN, NO_CONDITION  },
    {   "wallet_propose",       byRef (&doWalletPropose),     Role::USER,  NO_CONDITION  },
    {   "wallet_seed",          byRef (&doWalletSeed),        Role::ADMIN, NO_CONDITION  },

    // Evented methods
    {   "subscribe",            byRef (&doSubscribe),         Role::USER,  NO_CONDITION  },
    {   "unsubscribe",          byRef (&doUnsubscribe),       Role::USER,  NO_CONDITION  },
    {   "sc",                   byRef (&doSmartContract),     Role::USER,  NO_CONDITION  },
    {   "download",             byRef (&doFileDownload),      Role::USER,  NO_CONDITION  },

    {  "get_token_info",        byRef (&doTokenInfo),         Role::USER,  NO_CONDITION  },
    {  "get_asset_balance",     byRef (&doAssetBalance),      Role::USER,  NO_CONDITION  },
    {  "get_asset_info",        byRef (&doAssetInfo),         Role::USER,  NO_CONDITION  },
    {  "is_asset_operator",     byRef (&doIsAssetOperator),   Role::USER,  NO_CONDITION  },
    {  "get_asset_all_token_info",  byRef (&doAssetAllTokenInfo),    Role::USER,  NO_CONDITION  },
    {  "get_account_all_asset_info",    byRef (&doAccountAllAssetInfo),      Role::USER,  NO_CONDITION  },
    {  "get_account_all_token_info",    byRef (&doAccountAllTokenInfo),      Role::USER,  NO_CONDITION  },
};

} // namespace

const Handler* getHandler(std::string const& name) {
    static HandlerTable const handlers(handlerArray);
    return handlers.getHandler(name);
}

} // RPC
} //