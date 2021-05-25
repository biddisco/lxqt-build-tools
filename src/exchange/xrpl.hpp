#include <algorithm>
#include <string>
#include <iostream>
//
#include <ripple/protocol/KeyType.h>

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
        int64_t amount,
        const std::string &currency,
        const std::string &issuer);
