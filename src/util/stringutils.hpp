#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/all.hpp>

#define JCHARP(val) val.get_ptr<json::string_t*>()->c_str()

// ----------------------------------------------------------------------------
// returns a lowercase copy of the input string
std::string lowercase(std::string data) {
    std::transform(data.begin(), data.end(), data.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return data;
}

// ----------------------------------------------------------------------------
// in place conversion of string to lowercase
void lowercase_i(std::string &data) {
    std::transform(data.begin(), data.end(), data.begin(),
        [](unsigned char c){ return std::tolower(c); });
}


// ----------------------------------------------------------------------------
// Function to transform a range into a std::string
// Replace this with 'std::string_view' to make it a view instead.
auto make_string = [](auto&& r) -> std::string_view {
    const auto data = &*r.begin();
    const auto size = static_cast<std::size_t>(ranges::distance(r));
    return std::string_view{data, size};
};

std::pair<std::string_view, std::string_view> get_currency_pair(const std::string& str)
{
    const auto range = str |
                       ranges::views::split('/') |
                       ranges::views::transform(make_string);
    return std::make_pair(ranges::front(range), *next(ranges::begin(range)));
}

//std::pair<std::string, std::string> get_currency_pair(const std::string &cs)
//{
//    std::size_t pos = cs.find("/");
//    std::string c1 = cs.substr(0,pos);
//    std::string c2 = cs.substr(pos+1);
//    return std::make_pair(c1, c2);
//}

//std::pair<std::string, std::string> get_currency_pair(const char *cp)
//{
//    std::size_t pos = strpos(cp, "/");
//    std::string c1 = std::string(cp, pos);
//    std::string c2 = std::string(cp[pos], .substr(pos+1);
//    return std::make_pair(c1, c2);
//}
