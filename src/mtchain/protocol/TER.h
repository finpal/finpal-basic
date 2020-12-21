//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#ifndef MTCHAIN_PROTOCOL_TER_H_INCLUDED
#define MTCHAIN_PROTOCOL_TER_H_INCLUDED

#include <string>

namespace mtchain {

// See https://mtchain.io/wiki/Transaction_errors
//
// "Transaction Engine Result"
// or Transaction ERror.
//
enum TER
{
    // Note: Range is stable.  Exact numbers are currently unstable.  Use tokens.

    // -399 .. -300: L Local error (transaction fee inadequate, exceeds local limit)
    // Only valid during non-consensus processing.
    // Implications:
    // - Not forwarded
    // - No fee check
    telLOCAL_ERROR  = -399,
    telBAD_DOMAIN,
    telBAD_PATH_COUNT,
    telBAD_PUBLIC_KEY,
    telFAILED_PROCESSING,
    telINSUF_FEE_P,
    telNO_DST_PARTIAL,
    telCAN_NOT_QUEUE,

    // -299 .. -200: M Malformed (bad signature)
    // Causes:
    // - Transaction corrupt.
    // Implications:
    // - Not applied
    // - Not forwarded
    // - Reject
    // - Can not succeed in any imagined ledger.
    temMALFORMED    = -299,

    temBAD_AMOUNT,
    temBAD_CURRENCY,
    temBAD_EXPIRATION,
    temBAD_FEE,
    temBAD_ISSUER,
    temBAD_LIMIT,
    temBAD_OFFER,
    temBAD_PATH,
    temBAD_PATH_LOOP,
    temBAD_SEND_M_LIMIT,
    temBAD_SEND_M_MAX,
    temBAD_SEND_M_NO_DIRECT,
    temBAD_SEND_M_PARTIAL,
    temBAD_SEND_M_PATHS,
    temBAD_SEQUENCE,
    temBAD_SIGNATURE,
    temBAD_SRC_ACCOUNT,
    temBAD_TRANSFER_RATE,
    temDST_IS_SRC,
    temDST_NEEDED,
    temINVALID,
    temINVALID_FLAG,
    temREDUNDANT,
    temMTCHAIN_EMPTY,
    temDISABLED,
    temBAD_SIGNER,
    temBAD_QUORUM,
    temBAD_WEIGHT,
    temBAD_TICK_SIZE,
    temBAD_ASSET,
    temBAD_TOKEN,

    // An intermediate result used internally, should never be returned.
    temUNCERTAIN,
    temUNKNOWN,

    // -199 .. -100: F
    //    Failure (sequence number previously used)
    //
    // Causes:
    // - Transaction cannot succeed because of ledger state.
    // - Unexpected ledger state.
    // - C++ exception.
    //
    // Implications:
    // - Not applied
    // - Not forwarded
    // - Could succeed in an imagined ledger.
    tefFAILURE      = -199,
    tefALREADY,
    tefBAD_ADD_AUTH,
    tefBAD_AUTH,
    tefBAD_LEDGER,
    tefCREATED,
    tefEXCEPTION,
    tefINTERNAL,
    tefNO_AUTH_REQUIRED,    // Can't set auth if auth is not required.
    tefPAST_SEQ,
    tefWRONG_PRIOR,
    tefMASTER_DISABLED,
    tefMAX_LEDGER,
    tefBAD_SIGNATURE,
    tefBAD_QUORUM,
    tefNOT_MULTI_SIGNING,
    tefBAD_AUTH_MASTER,
    tefASSET_EXIST,
    tefNO_OWNER,
    tefNO_ASSET,
    tefTOKEN_EXIST,
    tefNO_TOKEN,
    tefDST_IS_OWNER,
    tefDESTROY_ASSET,


    // -99 .. -1: R Retry
    //   sequence too high, no funds for txn fee, originating -account
    //   non-existent
    //
    // Cause:
    //   Prior application of another, possibly non-existent, transaction could
    //   allow this transaction to succeed.
    //
    // Implications:
    // - Not applied
    // - May be forwarded
    //   - Results indicating the txn was forwarded: terQUEUED
    //   - All others are not forwarded.
    // - Might succeed later
    // - Hold
    // - Makes hole in sequence which jams transactions.
    terRETRY        = -99,
    terFUNDS_SPENT,      // This is a free transaction, so don't burden network.
    terINSUF_FEE_B,      // Can't pay fee, therefore don't burden network.
    terNO_ACCOUNT,       // Can't pay fee, therefore don't burden network.
    terNO_AUTH,          // Not authorized to hold IOUs.
    terNO_LINE,          // Internal flag.
    terOWNERS,           // Can't succeed with non-zero owner count.
    terPRE_SEQ,          // Can't pay fee, no point in forwarding, so don't
                         // burden network.
    terLAST,             // Process after all other transactions
    terNO_MTCHAIN,       // Rippling not allowed
    terQUEUED,           // Transaction is being held in TxQ until fee drops

    // 0: S Success (success)
    // Causes:
    // - Success.
    // Implications:
    // - Applied
    // - Forwarded
    tesSUCCESS      = 0,

    // 100 .. 159 C
    //   Claim fee only (mtchain transaction with no good paths, pay to
    //   non-existent account, no path)
    //
    // Causes:
    // - Success, but does not achieve optimal result.
    // - Invalid transaction or no effect, but claim fee to use the sequence
    //   number.
    //
    // Implications:
    // - Applied
    // - Forwarded
    //
    // Only allowed as a return code of appliedTransaction when !tapRetry.
    // Otherwise, treated as terRETRY.
    //
    // DO NOT CHANGE THESE NUMBERS: They appear in ledger meta data.
    tecCLAIM                    = 100,
    tecPATH_PARTIAL             = 101,
    tecUNFUNDED_ADD             = 102,
    tecUNFUNDED_OFFER           = 103,
    tecUNFUNDED_PAYMENT         = 104,
    tecFAILED_PROCESSING        = 105,
    tecUNFUNDED_DEAL_OFFER      = 106,
    tecFAILED_IOU_FEE           = 107,
    tecNO_LINE_IOU_FEE          = 108,
    tecEXCEED_TRUST_LIMIT       = 109,
    tecUNFUNDED_PAYMENT_IOU     = 110,
    tecUNFUNDED_PAYMENT_IOU_FEE = 111,
    tecIOU_PRECISION_MISMATCH   = 112,
    tecFORBID_TRUST_LINE        = 113,
    tecDIR_FULL                 = 121,
    tecINSUF_RESERVE_LINE       = 122,
    tecINSUF_RESERVE_OFFER      = 123,
    tecNO_DST                   = 124,
    tecNO_DST_INSUF_M           = 125,
    tecNO_LINE_INSUF_RESERVE    = 126,
    tecNO_LINE_REDUNDANT        = 127,
    tecPATH_DRY                 = 128,
    tecUNFUNDED                 = 129,  // Deprecated, old ambiguous unfunded.
    tecNO_ALTERNATIVE_KEY       = 130,
    tecNO_REGULAR_KEY           = 131,
    tecOWNERS                   = 132,
    tecNO_ISSUER                = 133,
    tecNO_AUTH                  = 134,
    tecNO_LINE                  = 135,
    tecINSUFF_FEE               = 136,
    tecFROZEN                   = 137,
    tecNO_TARGET                = 138,
    tecNO_PERMISSION            = 139,
    tecNO_ENTRY                 = 140,
    tecINSUFFICIENT_RESERVE     = 141,
    tecNEED_MASTER_KEY          = 142,
    tecDST_TAG_NEEDED           = 143,
    tecINTERNAL                 = 144,
    tecOVERSIZE                 = 145,
    tecCRYPTOCONDITION_ERROR    = 146,
    tecOVERISSUE                = 147
};

inline bool isTelLocal(TER x)
{
    return ((x) >= telLOCAL_ERROR && (x) < temMALFORMED);
}

inline bool isTemMalformed(TER x)
{
    return ((x) >= temMALFORMED && (x) < tefFAILURE);
}

inline bool isTefFailure(TER x)
{
    return ((x) >= tefFAILURE && (x) < terRETRY);
}

inline bool isTerRetry(TER x)
{
    return ((x) >= terRETRY && (x) < tesSUCCESS);
}

inline bool isTesSuccess(TER x)
{
    return ((x) == tesSUCCESS);
}

inline bool isTecClaim(TER x)
{
    return ((x) >= tecCLAIM);
}

// VFALCO TODO group these into a shell class along with the defines above.
extern
bool
transResultInfo (TER code, std::string& token, std::string& text);

extern
std::string
transToken (TER code);

extern
std::string
transHuman (TER code);

} //

#endif