// STL
#include <algorithm>
#include <string>
#include <iostream>
// Grox
#include "src/debug.hpp"
// extern
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/BuildInfo.h>
#include <ripple/protocol/HashPrefix.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/Sign.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/tokens.h>

// ----------------------------------------------------------------------------
std::string serialize(ripple::STTx const& tx)
{
    using namespace ripple;
    return strHex(tx.getSerializer().peekData());
}

// ----------------------------------------------------------------------------
std::shared_ptr<ripple::STTx const> deserialize(std::string blob)
{
    using namespace ripple;
    auto ret{strUnHex(blob)};
    if (!ret.has_value() || ret.get_ptr()->size() == 0)
        Throw<std::runtime_error>("transaction not valid hex");

    SerialIter sitTrans{makeSlice(ret.get())};
    // Can Throw
    return std::make_shared<STTx const>(std::ref(sitTrans));
}

// ----------------------------------------------------------------------------
#define DEBUG_TX_SIGN
std::string make_xrp_payment(
        ripple::KeyType keyType,
        const std::string &from_seed,
        const std::string &from_address,
        int32_t from_sequence,
        const std::string &dest_address,
        int32_t dest_tag,
        int64_t amount,
        const std::string &currency,
        const std::string &issuer)
{
    using namespace ripple;
    //
    auto const seed = parseGenericSeed(from_seed);
    assert(seed);
    auto const keypair = generateKeyPair(keyType, *seed);
    auto const id = calcAccountID(keypair.first);
    assert(toBase58(id) == from_address);

    auto const destination = parseBase58<AccountID>(dest_address);
    assert(destination);
    auto const gateway1 = parseBase58<AccountID>(issuer);
    if (currency!="") {
        assert(gateway1);
    }

    STTx payTx(ttPAYMENT, [&](auto& obj) {
        // General transaction fields
        obj[sfAccount] = id;
        obj[sfFee] = STAmount{100};
        obj[sfFlags] = tfFullyCanonicalSig;
        obj[sfSigningPubKey] = keypair.first.slice();
        obj[sfSequence] = from_sequence;
        // Payment-specific fields
        obj[sfDestination] = *destination;
        obj[sfDestinationTag] = dest_tag;
        if (currency.size()>0) {
            obj[sfAmount]  = STAmount(Issue(to_currency(currency), *gateway1), amount, -2);
            // we multiply by 1.002 to allow for IOU fees, and scale the float to int size,
            // but shift right by the same amount to move the decimal point back to dollars.cents
            obj[sfSendMax] = STAmount(Issue(to_currency(currency), *gateway1), static_cast<uint64_t>(amount*1.002*1E5), -(2+5));
        }
        else {
            obj[sfAmount] = STAmount(XRPAmount(amount)); // drops?
        }
    });

    DEBUG_ONLY("Before signing: \n"
              << payTx.getJson(JsonOptions::none).toStyledString() << std::endl
              << "Serialized: " << payTx.getJson(JsonOptions::none, true)[jss::tx]);

    payTx.sign(keypair.first, keypair.second);

    auto const serialized = serialize(payTx);

#ifdef DEBUG_TX_SIGN
    std::cout << "\nAfter signing: \n"
        << payTx.getJson(JsonOptions::none).toStyledString() << std::endl
        << "Serialized: " << serialized << std::endl;
#endif

    return serialized;
}

// ----------------------------------------------------------------------------
std::string make_xrp_offer(
        ripple::KeyType keyType,
        const std::string &from_seed,
        const std::string &from_address,
        int32_t from_sequence,
        ripple::STAmount const& pays,
        ripple::STAmount const& gets,
        std::uint32_t flags)
{
    using namespace ripple;
    //
    auto const seed = parseGenericSeed(from_seed);
    auto const keypair = generateKeyPair(keyType, *seed);
    auto const id = calcAccountID(keypair.first);
    assert(toBase58(id) == from_address);

    STTx offerTx(ttOFFER_CREATE, [&](auto& obj) {
        // General transaction fields
        obj[sfAccount] = id;
        obj[sfFee] = STAmount{100};
        obj[sfFlags] = tfFullyCanonicalSig;
        if (flags)
            obj[sfFlags] = flags;
        obj[sfSigningPubKey] = keypair.first.slice();
        obj[sfSequence] = from_sequence;
        // Offer specific fields
        obj[sfTakerPays] = pays;
        obj[sfTakerGets] = gets;
    });

    DEBUG_ALWAYS("Before signing: \n"
              << offerTx.getJson(JsonOptions::none).toStyledString() << std::endl
              << "Serialized: " << offerTx.getJson(JsonOptions::none, true)[jss::tx]);

    offerTx.sign(keypair.first, keypair.second);

    auto const serialized = serialize(offerTx);

#ifdef DEBUG_TX_SIGN
    std::cout << "\nAfter signing: \n"
        << offerTx.getJson(JsonOptions::none).toStyledString() << std::endl
        << "Serialized: " << serialized << std::endl;
#endif

    return serialized;
}

// ----------------------------------------------------------------------------
std::string cancel_xrp_offer(
        ripple::KeyType keyType,
        const std::string &from_seed,
        const std::string &from_address,
        int32_t from_sequence,
        int32_t offerSeq,
        std::uint32_t flags)
{
    using namespace ripple;
    //
    auto const seed = parseGenericSeed(from_seed);
    auto const keypair = generateKeyPair(keyType, *seed);
    auto const id = calcAccountID(keypair.first);
    assert(toBase58(id) == from_address);

    STTx offerTx(ttOFFER_CANCEL, [&](auto& obj) {
        // General transaction fields
        obj[sfAccount] = id;
        obj[sfFee] = STAmount{100};
        if (flags)
            obj[sfFlags] = flags;
        obj[sfSigningPubKey] = keypair.first.slice();
        obj[sfSequence] = from_sequence;
        // Offer specific fields
        obj[sfOfferSequence] = offerSeq;
    });

    DEBUG_ALWAYS("Before signing: \n"
              << offerTx.getJson(JsonOptions::none).toStyledString() << std::endl
              << "Serialized: " << offerTx.getJson(JsonOptions::none, true)[jss::tx]);

    offerTx.sign(keypair.first, keypair.second);

    auto const serialized = serialize(offerTx);

#ifdef DEBUG_TX_SIGN
    std::cout << "\nAfter signing: \n"
        << offerTx.getJson(JsonOptions::none).toStyledString() << std::endl
        << "Serialized: " << serialized << std::endl;
#endif

    return serialized;
}
