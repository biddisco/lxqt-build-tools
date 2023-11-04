// STL
#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
// Grox
#include "currency/currency.hpp"
#include "debug/print.hpp"
// extern
#include <ripple/basics/StringUtilities.h>
#include <ripple/basics/strHex.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/BuildInfo.h>
#include <ripple/protocol/HashPrefix.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/Sign.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/tokens.h>

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> xrpl_dbg("XRPLFunc");

// ----------------------------------------------------------------------------
std::string currency_to_hex(std::string_view currency)
{
  if (currency.size() > 3)
  {
    std::string hexcode = ripple::strHex(currency.begin(), currency.end());
    while (hexcode.size() < 40)
      hexcode += '0';
    return hexcode;
  }
  else if (currency.size() < 3)
  {
    return std::string(currency);
  }
  return std::string(currency);
}

// ----------------------------------------------------------------------------
std::string hex_to_currency(std::string_view hex)
{
  if (hex.size() == 40)
  {
    while (hex.back() == '0')
      hex = hex.substr(0, hex.size() - 1);
    auto code = ripple::strUnHex(hex.size(), hex.begin(), hex.end());
    if (code.has_value())
    {
      std::string result =
        std::string(&code.value().data()[0], &code.value().data()[code.value().size()]);
      return result;
    }
  }
  return std::string(hex);
}

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
  if (!ret.has_value() || ret.value().size() == 0)
    Throw<std::runtime_error>("transaction not valid hex");

  SerialIter sitTrans{makeSlice(ret.value())};
  // Can Throw
  return std::make_shared<STTx const>(std::ref(sitTrans));
}

// ----------------------------------------------------------------------------
#define DEBUG_TX_SIGN 1

std::string make_xrp_payment(ripple::KeyType keyType, std::string const& from_seed,
  std::string const& from_address, int32_t from_sequence, std::string const& dest_address,
  int32_t dest_tag, double amount, std::string const& currency, std::string const& issuer,
  double transferrate)
{
  using namespace ripple;
  // get from account keys/info
  auto const seed = parseGenericSeed(from_seed);
  assert(seed);
  auto const keypair = generateKeyPair(keyType, *seed);
  auto const id = calcAccountID(keypair.first);
  assert(toBase58(id) == from_address);

  // get to account info
  auto const destination = parseBase58<AccountID>(dest_address);
  assert(destination);

  // currency issuer - is this an IOU
  auto const gateway1 = parseBase58<AccountID>(issuer);
  bool currency_xrp = true;
  if (currency != "")
  {
    assert(gateway1);
    currency_xrp = false;
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
    if (currency_xrp)
    {
      obj[sfAmount] =
        STAmount(Issue(to_currency(currency), *gateway1), static_cast<uint64_t>(amount));
    }
    else if (is_fiat(issued_currency{issuer, currency}))
    {
      // amount we want to send as dollars.cents, multiply x 100, shift right 2 places
      obj[sfAmount] =
        STAmount(Issue(to_currency(currency), *gateway1), static_cast<uint64_t>(amount * 1E2), -2);
      // we multiply by 1 + transferrate (eg. 1.002) to allow for IOU fees, and scale the float to int size,
      // but shift right by the same amount to move the decimal point back to dollars.cents
      obj[sfSendMax] = STAmount(Issue(to_currency(currency), *gateway1),
        static_cast<uint64_t>(amount * (1.0 + 0.01 * transferrate) * 1E5), -(2 + 5));
    }
    else
    {
      // on IOUs a transfer rate (say 1.0001 = 0.01%) might apply,
      // the receiver will only get R = (X - fee)
      // no fee when returning a token to its issuer
      double recv_amount = amount;
      if (transferrate > 0 && dest_address != issuer)
      {
        recv_amount = amount / (1.0 + amount * 0.01 * transferrate);
      }

      obj[sfAmount] = STAmount(
        Issue(to_currency(currency), *gateway1), static_cast<uint64_t>(1E6 * recv_amount), -6);

      obj[sfSendMax] =
        STAmount(Issue(to_currency(currency), *gateway1), static_cast<uint64_t>(1E6 * amount), -6);
    }
  });

  xrpl_dbg<0>.debug(str<>("payment"), "Before signing: \n",
    payTx.getJson(JsonOptions::none).toStyledString(), "\n",
    "Serialized:", payTx.getJson(JsonOptions::none, true)[jss::tx].asString());

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
std::string make_xrp_offer(ripple::KeyType keyType, std::string const& from_seed,
  std::string const& from_address, int32_t from_sequence, ripple::STAmount const& pays,
  ripple::STAmount const& gets, std::uint32_t flags)
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

  xrpl_dbg<0>.debug(str<>("offer"), "Before signing: \n",
    offerTx.getJson(JsonOptions::none).toStyledString(), "\n",
    "Serialized:", offerTx.getJson(JsonOptions::none, true)[jss::tx]);

  offerTx.sign(keypair.first, keypair.second);

  auto const serialized = serialize(offerTx);

#ifdef DEBUG_TX_SIGN
  std::cout << "\nAfter signing: \n"
            << offerTx.getJson(JsonOptions::none).toStyledString() << std::endl
            << "Serialized:" << serialized << std::endl;
#endif

  return serialized;
}

// ----------------------------------------------------------------------------
std::string cancel_xrp_offer(ripple::KeyType keyType, std::string const& from_seed,
  std::string const& from_address, int32_t from_sequence, int32_t offerSeq, std::uint32_t flags)
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

  xrpl_dbg<0>.debug(str<>("offer cancel"), "Before signing: \n",
    offerTx.getJson(JsonOptions::none).toStyledString(), "\n",
    "Serialized:", offerTx.getJson(JsonOptions::none, true)[jss::tx]);

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
std::string set_trustline(ripple::KeyType keyType, std::string const& from_seed,
  std::string const& from_address, int32_t from_sequence, int64_t limit,
  std::string const& currency, std::string const& issuer, std::uint32_t flags)
{
  using namespace ripple;
  //
  auto const seed = parseGenericSeed(from_seed);
  auto const keypair = generateKeyPair(keyType, *seed);
  auto const id = calcAccountID(keypair.first);
  assert(toBase58(id) == from_address);

  auto const gateway1 = parseBase58<AccountID>(issuer);
  if (currency != "")
  {
    assert(gateway1);
  }

  std::string hexcode = currency_to_hex(currency);

  STTx offerTx(ttTRUST_SET, [&](auto& obj) {
    // General transaction fields
    obj[sfAccount] = id;
    obj[sfFee] = STAmount{100};
    if (flags)
      obj[sfFlags] = flags;
    obj[sfSigningPubKey] = keypair.first.slice();
    obj[sfSequence] = from_sequence;

    // Trustline specific fields
    if (currency.size() > 0)
    {
      STAmount LimitAmount = STAmount(Issue(to_currency(hexcode), *gateway1), limit);
      obj[sfLimitAmount] = LimitAmount;
    }
  });

  xrpl_dbg<0>.debug(str<>("trustline"), "Before signing: \n",
    offerTx.getJson(JsonOptions::none).toStyledString(), "\n",
    "Serialized:", offerTx.getJson(JsonOptions::none, true)[jss::tx]);

  offerTx.sign(keypair.first, keypair.second);

  auto const serialized = serialize(offerTx);

#ifdef DEBUG_TX_SIGN
  std::cout << "\nAfter signing: \n"
            << offerTx.getJson(JsonOptions::none).toStyledString() << std::endl
            << "Serialized: " << serialized << std::endl;
#endif

  return serialized;
}
