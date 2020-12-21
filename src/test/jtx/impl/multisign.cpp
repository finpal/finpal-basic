//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <test/jtx/multisign.h>
#include <test/jtx/utility.h>
#include <mtchain/protocol/HashPrefix.h>
#include <mtchain/protocol/JsonFields.h>
#include <mtchain/protocol/Sign.h>
#include <mtchain/protocol/types.h>
#include <mtchain/basics/contract.h>

namespace mtchain {
namespace test {
namespace jtx {

Json::Value
signers (Account const& account,
    std::uint32_t quorum,
        std::vector<signer> const& v)
{
    Json::Value jv;
    jv[jss::Account] = account.human();
    jv[jss::TransactionType] = "SignerListSet";
    jv[sfSignerQuorum.getJsonName()] = quorum;
    auto& ja = jv[sfSignerEntries.getJsonName()];
    ja.resize(v.size());
    for(std::size_t i = 0; i < v.size(); ++i)
    {
        auto const& e = v[i];
        auto& je = ja[i][sfSignerEntry.getJsonName()];
        je[jss::Account] = e.account.human();
        je[sfSignerWeight.getJsonName()] = e.weight;
    }
    return jv;
}

Json::Value
signers (Account const& account, none_t)
{
    Json::Value jv;
    jv[jss::Account] = account.human();
    jv[jss::TransactionType] = "SignerListSet";
    jv[sfSignerQuorum.getJsonName()] = 0;
    return jv;
}

//------------------------------------------------------------------------------

msig::msig (std::vector<msig::Reg> signers_)
        : signers(std::move(signers_))
{
    // Signatures must be applied in sorted order.
    std::sort(signers.begin(), signers.end(),
        [](msig::Reg const& lhs, msig::Reg const& rhs)
        {
            return lhs.acct.id() < rhs.acct.id();
        });
}

void
msig::operator()(Env& env, JTx& jt) const
{
    auto const mySigners = signers;
    jt.signer = [mySigners, &env](Env&, JTx& jt)
    {
        jt[sfSigningPubKey.getJsonName()] = "";
        boost::optional<STObject> st;
        try
        {
            st = parse(jt.jv);
        }
        catch(parse_error const&)
        {
            env.test.log << pretty(jt.jv) << std::endl;
            Rethrow();
        }
        auto& js = jt[sfSigners.getJsonName()];
        js.resize(mySigners.size());
        for(std::size_t i = 0; i < mySigners.size(); ++i)
        {
            auto const& e = mySigners[i];
            auto& jo = js[i][sfSigner.getJsonName()];
            jo[jss::Account] = e.acct.human();
            jo[jss::SigningPubKey] = strHex(e.sig.pk().slice());

            Serializer ss {buildMultiSigningData (*st, e.acct.id())};
            auto const sig = mtchain::sign (
                *publicKeyType(e.sig.pk().slice()), e.sig.sk(), ss.slice());
            jo[sfTxnSignature.getJsonName()] =
                strHex(Slice{ sig.data(), sig.size() });
        }
    };
}

} // jtx
} // test
} //
