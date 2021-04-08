#include <algorithm>
#include <string>
#include <iostream>
//
#include <ripple/protocol/KeyType.h>

// ----------------------------------------------------------------------------
bool make_xrp_payment(
        ripple::KeyType keyType,
        std::string secret_seed,
        std::string public_address,
        std::string dest_address,
        int dest_tag,
        int amount_drops);
