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
bool make_xrp_payment(
        ripple::KeyType keyType,
        std::string secret_seed,
        std::string public_address,
        std::string dest_address,
        int dest_tag,
        int amount_drops)
{
    using namespace ripple;
    //
    auto const seed = parseGenericSeed(secret_seed);
    assert(seed);
    auto const keypair = generateKeyPair(keyType, *seed);
    auto const id = calcAccountID(keypair.first);
    assert(toBase58(id) == public_address);
    //
    std::cout << std::endl
              << to_string(keyType) << " secret \"" << secret_seed
              << "\" generates secret key \"" << toBase58(*seed) << "\" and public key \""
              << toBase58(id) << std::endl;

    auto const destination = parseBase58<AccountID>(dest_address);
    assert(destination);
    STTx noopTx(ttPAYMENT, [&](auto& obj) {
        // General transaction fields
        obj[sfAccount] = id;
        obj[sfFee] = STAmount{100};
        obj[sfFlags] = tfFullyCanonicalSig;
        obj[sfSigningPubKey] = keypair.first.slice();
        // Payment-specific fields
        obj[sfDestination] = *destination;
        obj[sfDestinationTag] = dest_tag;
        obj[sfAmount] = STAmount(XRPAmount(amount_drops)); // drops?
        obj[sfSendMax] = STAmount(XRPAmount(amount_drops)); // drops?
    });

    std::cout << "\nBefore signing: \n"
              << noopTx.getJson(JsonOptions::none).toStyledString() << std::endl
              << "Serialized: " << noopTx.getJson(JsonOptions::none, true)[jss::tx]
              << std::endl;

    noopTx.sign(keypair.first, keypair.second);

    auto const serialized = serialize(noopTx);
    std::cout << "\nAfter signing: \n"
              << noopTx.getJson(JsonOptions::none).toStyledString() << std::endl
              << "Serialized: " << serialized << std::endl;

    auto const deserialized = deserialize(serialized);
    assert(deserialized);
    assert(deserialized->getTransactionID() == noopTx.getTransactionID());
    std::cout << "Deserialized: "
              << deserialized->getJson(JsonOptions::none).toStyledString() << std::endl;

    auto const check1 = noopTx.checkSign(STTx::RequireFullyCanonicalSig::no);

    std::cout << "Check 1: " << (check1.first ? "Good" : "Bad!") << std::endl;
    assert(check1.first);

    // Use the function primitives, which are hidden by the `STTx`
    // interface, to check the signature again and verify that
    // `STTx` is working properly.
    auto const& signatureSlice = noopTx[sfTxnSignature];
    Blob const data = [&] {
        // This is a copy of the static `getSigningData` function body
        // which is needed by `verify`.
        Serializer s;
        s.add32(HashPrefix::txSign);
        noopTx.addWithoutSigningFields(s);
        return s.getData();
    }();

    // STTx::checkSign calls `verify` indirectly via `checkSingleSign`
    auto const check2 = verify(keypair.first, makeSlice(data), signatureSlice, true);

    std::cout << "Check 2: " << (check2 ? "Good" : "Bad!") << std::endl;

    return check1.first && check2;
}

