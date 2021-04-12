#include <algorithm>
#include <string>
#include <iostream>
//
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

#ifndef DEBUG_ONLY
# define DEBUG_ONLY(x)
# define DEBUG_ALWAYS(x) { \
    std::stringstream temp; temp << x; \
    std::cout << temp.str() << std::endl; }
#endif

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
//#define DEBUG_TX_SIGN

std::string make_xrp_payment(
        ripple::KeyType keyType,
        std::string from_seed,
        std::string from_address,
        int32_t from_sequence,
        std::string dest_address,
        int32_t dest_tag,
        int64_t amount_drops)
{
    using namespace ripple;
    //
    auto const seed = parseGenericSeed(from_seed);
    assert(seed);
    auto const keypair = generateKeyPair(keyType, *seed);
    auto const id = calcAccountID(keypair.first);
    assert(toBase58(id) == from_address);

#ifdef DEBUG_TX_SIGN_SHOW_SECRET
    std::cout << std::endl
              << to_string(keyType) << /*" secret \"" << secret_seed
              << "\" generates secret key \"" << toBase58(*seed) << */"\" and public key \""
              << toBase58(id) << std::endl;
#endif

    auto const destination = parseBase58<AccountID>(dest_address);
    assert(destination);
    STTx noopTx(ttPAYMENT, [&](auto& obj) {
        // General transaction fields
        obj[sfAccount] = id;
        obj[sfFee] = STAmount{100};
        obj[sfFlags] = tfFullyCanonicalSig;
        obj[sfSigningPubKey] = keypair.first.slice();
        obj[sfSequence] = from_sequence;
        // Payment-specific fields
        obj[sfDestination] = *destination;
        obj[sfDestinationTag] = dest_tag;
        obj[sfAmount] = STAmount(XRPAmount(amount_drops)); // drops?
    });

    DEBUG_ONLY("Before signing: \n"
              << noopTx.getJson(JsonOptions::none).toStyledString() << std::endl
              << "Serialized: " << noopTx.getJson(JsonOptions::none, true)[jss::tx]);

    noopTx.sign(keypair.first, keypair.second);

    auto const serialized = serialize(noopTx);

#ifdef DEBUG_TX_SIGN
    std::cout << "\nAfter signing: \n"
        << noopTx.getJson(JsonOptions::none).toStyledString() << std::endl
        << "Serialized: " << serialized << std::endl;
#endif

    return serialized;
}

