#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
//
#include <QString>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/all.hpp>
#include <nlohmann/json.hpp>

#define JCHARP(val) val.get_ptr<nlohmann::json::string_t*>()->c_str()

/*
struct fmt::formatter<QString> : formatter<const char*> {
  auto format(QString const& s, format_context& ctx) {
    return formatter<const char*>::format((const char *)value.toUtf8(), ctx);
  }
};
*/

// ----------------------------------------------------------------------------
inline bool startswith(std::string_view str, std::string_view sub)
{
  // rev search - pos=0, limits search to pos or earlier
  // equivalent to if data.startswith(...)
  if (str.rfind(sub, 0) != 0) { return false; }
  return true;
}

// ----------------------------------------------------------------------------
inline bool startswith(QString str, QString sub) { return str.startsWith(sub); }

// ----------------------------------------------------------------------------
inline QString to_qstring(std::string const& str) { return QString::fromStdString(str); }

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
// Function to transform a range into a std::string or std::string_view
template <typename Result>
inline auto make_string = [](auto&& r) -> Result {
  auto const data = &*r.begin();
  auto const size = static_cast<std::size_t>(ranges::distance(r));
  return Result{data, size};
};

inline std::pair<std::string_view, std::string_view> split_currency_pair_string(
    std::string_view str, char const delim = '/')
{
  auto const range =
      str | ranges::views::split(delim) | ranges::views::transform(make_string<std::string_view>);
  return std::make_pair(ranges::front(range), *next(ranges::begin(range)));
}
