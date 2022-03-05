#include <algorithm>
#include <string>
#include <string_view>
#include <iostream>
//
#include <ripple/protocol/KeyType.h>
#include <ripple/protocol/STAmount.h>

// Useful functions that directly make use of ripple-lib
std::string hexcurrency(std::string_view name);

// ----------------------------------------------------------------------------
// When paying XRP, the amount is drops (1E6 x xrp)
// When paying IOUs, the amount is whole units of cents ($1 is 100)
std::string make_xrp_payment(
        ripple::KeyType keyType,
        const std::string &from_seed,
        const std::string &from_address,
        int32_t from_sequence,
        const std::string &dest_address,
        int32_t dest_tag,
        double amount,
        const std::string &currency,
        const std::string &issuer,
        double transferrate);

std::string make_xrp_offer(
        ripple::KeyType keyType,
        const std::string &from_seed,
        const std::string &from_address,
        int32_t from_sequence,
        ripple::STAmount const& pays,
        ripple::STAmount const& gets,
        std::uint32_t flags);

std::string cancel_xrp_offer(
        ripple::KeyType keyType,
        const std::string &from_seed,
        const std::string &from_address,
        int32_t from_sequence,
        int32_t offerSeq,
        std::uint32_t flags);

std::string set_trustline(
        ripple::KeyType keyType,
        const std::string &from_seed,
        const std::string &from_address,
        int32_t from_sequence,
        int64_t limit,
        const std::string &currency,
        const std::string &issuer,
        std::uint32_t flags);
