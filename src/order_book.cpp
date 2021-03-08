#include <string>
#include <string_view>
//
#include "order_book.hpp"

bool startswith(const std::string_view str, const std::string &sub)
{
    // rev search - pos=0, limits search to pos or earlier
    // equivalent to if data.startswith(...)
    if (str.rfind(sub, 0) != 0) {
        return false;
    }
    return true;
}
