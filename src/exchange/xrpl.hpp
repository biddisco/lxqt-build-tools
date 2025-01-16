#pragma once

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
//
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/STAmount.h>

// Useful functions that directly make use of ripple-lib
std::string hexcurrency(std::string_view name);

// ----------------------------------------------------------------------------
// When paying XRP, the amount is drops (1E6 x xrp)
// When paying IOUs, the amount is whole units of cents ($1 is 100)
std::string make_xrp_payment(ripple::KeyType keyType, std::string const& from_seed,
    std::string const& from_address, int32_t from_sequence, std::string const& dest_address,
    int32_t dest_tag, double amount, std::string const& currency, std::string const& issuer,
    double transferrate);

std::string make_xrp_offer(ripple::KeyType keyType, std::string const& from_seed,
    std::string const& from_address, int32_t from_sequence, ripple::STAmount const& pays,
    ripple::STAmount const& gets, std::uint32_t flags);

std::string cancel_xrp_offer(ripple::KeyType keyType, std::string const& from_seed,
    std::string const& from_address, int32_t from_sequence, int32_t offerSeq, std::uint32_t flags);

std::string set_trustline(ripple::KeyType keyType, std::string const& from_seed,
    std::string const& from_address, int32_t from_sequence, int64_t limit,
    std::string const& currency, std::string const& issuer, std::uint32_t flags);
