#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/all.hpp>

#define JCHARP(val) val.get_ptr<json::string_t*>()->c_str()

// ----------------------------------------------------------------------------
inline bool startswith(std::string_view str, std::string_view sub)
{
  // rev search - pos=0, limits search to pos or earlier
  // equivalent to if data.startswith(...)
  if (str.rfind(sub, 0) != 0)
  {
    return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
inline std::string string_join(std::string_view s1, std::string_view s2)
{
  std::string result;
  result.reserve(s1.size() + s2.size() + 1);
  (result += s1) += s2;
  return result;
}

// ----------------------------------------------------------------------------
// returns a lowercase copy of the input string
inline std::string lowercase(std::string data)
{
  std::transform(
    data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::tolower(c); });
  return data;
}

// ----------------------------------------------------------------------------
// in place conversion of string to lowercase
inline void lowercase_i(std::string& data)
{
  std::transform(
    data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::tolower(c); });
}

// ----------------------------------------------------------------------------
// returns a uppercase copy of the input string
inline std::string uppercase(std::string data)
{
  std::transform(
    data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::toupper(c); });
  return data;
}

// ----------------------------------------------------------------------------
// in place conversion of string to uppercase
inline void uppercase_i(std::string& data)
{
  std::transform(
    data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::toupper(c); });
}

// ----------------------------------------------------------------------------
// Function to transform a range into a std::string
// Replace this with 'std::string_view' to make it a view instead.
inline auto make_string = [](auto&& r) -> std::string_view {
  const auto data = &*r.begin();
  const auto size = static_cast<std::size_t>(ranges::distance(r));
  return std::string_view{data, size};
};

inline std::pair<std::string_view, std::string_view> get_currency_pair(std::string_view str)
{
  const auto range = str | ranges::views::split('/') | ranges::views::transform(make_string);
  return std::make_pair(ranges::front(range), *next(ranges::begin(range)));
}
