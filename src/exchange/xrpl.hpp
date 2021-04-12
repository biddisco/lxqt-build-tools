#include <algorithm>
#include <string>
#include <iostream>
//
#include <ripple/protocol/KeyType.h>

// ----------------------------------------------------------------------------
std::string make_xrp_payment(
        ripple::KeyType keyType,
        std::string from_seed,
        std::string from_address,
        int32_t from_sequence,
        std::string dest_address,
        int32_t dest_tag,
        int64_t amount_drops);
