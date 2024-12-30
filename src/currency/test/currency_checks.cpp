#include <bitset>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
//
#include <QString>
//
#include <fmt/format.h>
//
#include "currency/currency.hpp"
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"
//
#include <gtest/gtest.h>

int main(int argc, char** argv);

using namespace grox;
using namespace grox::debug;
using namespace grox::debug::detail;

// ------------------------------------------------------------------
std::string diff(std::string const& s1, std::string const& s2)
{
  std::stringstream tmp;
  for (std::size_t index = 0; index < s1.size(); index++)
  {
    if (index < s2.size())
    {
      if (s1[index] == s2[index])
        tmp << (s1[index] == '\n' ? "\n" : ".");
      else
        tmp << s1[index];
    }
  }
  tmp << std::endl;
  for (std::size_t index = 0; index < s2.size(); index++)
  {
    if (index < s1.size())
    {
      if (s1[index] == s2[index])
        tmp << (s2[index] == '\n' ? "\n" : ".");
      else
        tmp << s2[index];
    }
  }
  return tmp.str();
}

// ------------------------------------------------------------------
bool compare(std::string const& expected, std::string const& received)
{
  if (bool ok = (expected == received))
  {
    std::cout << "passed : \"" << expected << "\"" << std::endl;
    return ok;
  }
  else
  {
    // clang-format off
    std::cout
        << "expected : " << expected.size() << "\n" << expected << "\n"
        << "received : " << received.size() << "\n" << received << "\n"
        << diff(expected, received) << std::endl;
    // clang-format on
    return ok;
  }
}

// ------------------------------------------------------------------
TEST(currency, currency_code)
{
  {
    currency_code c1{"", "USD"};
    currency_code c2{currency::bitstamp_trust, "USD"};
    std::stringstream s1;
    s1 << c1;
    EXPECT_TRUE(compare(s1.str(), "USD"));
    std::stringstream s2;
    s2 << c2;
    EXPECT_TRUE(compare(s2.str(), "USD.bitstamp"));
  }
  {
    currency_code c1{"", "USD"};
    currency_code c2{"", "USD"};
    EXPECT_TRUE(c1 == c2);
  }
  {
    currency_code c1{currency::bitstamp_trust, "USD"};
    currency_code c2{currency::bitstamp_trust, "USD"};
    EXPECT_TRUE(c1 == c2);
  }
  {
    currency_code c1{"", "USD"};
    currency_code c2{currency::bitstamp_trust, "USD"};
    currency_code c3{"", "XRP"};
    EXPECT_TRUE(c1.is_fiat());
    EXPECT_FALSE(c2.is_fiat());
    EXPECT_TRUE(c3.is_xrp());
  }
}

// ------------------------------------------------------------------
TEST(currency, currency)
{
  {
    currency c1{"", "USD"};
    currency c2{"", "USD"};
    EXPECT_TRUE(c1 == c2);
  }
  {
    currency c1{currency::bitstamp_trust, "USD"};
    currency c2{currency::bitstamp_trust, "USD"};
    EXPECT_TRUE(c1 == c2);
  }
  {
    currency c1{"", "ABC"};
    currency c2{"", "BCD"};
    EXPECT_TRUE(c1 < c2);
  }
  {
    currency c1{"", "USD"};
    currency c2{currency::bitstamp_trust, "USD"};
    //
    std::stringstream s1;
    s1 << c1;
    EXPECT_TRUE(compare("USD(0)", s1.str()));
    //
    std::stringstream s2;
    s2 << c2;
    EXPECT_TRUE(compare("USD(0)", s2.str()));
  }
  {
    currency c1{currency::bitstamp_trust, "USD"};
    EXPECT_TRUE(compare(c1.to_stringrep(), "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"));
    EXPECT_TRUE(compare(c1.to_stringrep(false), "USD"));
    EXPECT_FALSE(compare(c1.to_stringrep(), "USD"));
    EXPECT_FALSE(compare(c1.to_stringrep(false), "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"));
  }
  {
    currency c1{currency::bitstamp_trust, "USD"};
    EXPECT_TRUE(compare(c1.to_stringrep(), "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"));
    EXPECT_TRUE(compare(c1.to_stringrep(false), "USD"));
    EXPECT_FALSE(compare(c1.to_stringrep(), "USD"));
    EXPECT_FALSE(compare(c1.to_stringrep(false), "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"));
  }
  {
    currency c1{"", "USD"};
    currency c2{currency::bitstamp_trust, "USD"};
    currency c3{"", "XRP"};
    EXPECT_TRUE(c1.is_fiat());
    EXPECT_FALSE(c2.is_fiat());
    EXPECT_TRUE(c3.is_xrp());
  }
  {
    currency c1{"", "USD"};
    std::string str1 = to_string(534.123456, c1);
    EXPECT_TRUE(compare(str1, "534.12"));
    //
    currency c2{currency::bitstamp_trust, "USD"};
    std::string str2 = to_string(534.123456, c2);
    EXPECT_TRUE(compare(str2, "534.123456"));
  }
}

// ------------------------------------------------------------------
TEST(currency, currency_pair)
{
  {
    currency c1{currency::bitstamp_trust, "USD"};
    currency c2{currency::bitstamp_trust, "XRP"};
    currency_pair cp1{c1, c2};
    currency_pair cp2{c1, c2};
    EXPECT_TRUE(cp1 == cp2);
  }
  {
    currency_pair cp1{{currency::bitstamp_trust, "USD"}, {currency::bitstamp_trust, "XRP"}};
    currency_pair cp2{{currency::bitstamp_trust, "EUR"}, {currency::bitstamp_trust, "XRP"}};
    EXPECT_TRUE(cp2 < cp1);
    EXPECT_FALSE(cp2 > cp1);
  }
  {
    currency_pair cp1{{"", "USD"}, {"", "XRP"}};
    currency_pair cp2{{currency::bitstamp_trust, "USD"}, {"", "XRP"}};
    currency_pair cp3{{currency::bitstamp_trust, "EUR"}, {currency::ripple_trust, "USD"}};
    //
    std::string str1 = currency_pair_string(cp1, "/");
    EXPECT_TRUE(str1 == "USD/XRP");
    //
    std::string str2 = currency_pair_string(cp2, "-");
    EXPECT_TRUE(str2 == "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B-XRP");
    //
    std::string str3 = currency_pair_string(cp3, ":");
    EXPECT_TRUE(compare(
        str3, "EUR.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B:USD.rMxCKbEDwqr76QuheSUMdEGf4B9xJ8m5De"));
  }
  {
    currency_pair cp1{{"", "USD"}, {"", "XRP"}};
    currency_pair cp2{{currency::bitstamp_trust, "USD"}, {"", "XRP"}};
    currency_pair cp3{{currency::bitstamp_trust, "EUR"}, {currency::ripple_trust, "USD"}};
    //
    std::string str1 = currency_pair_string(cp1, "/");
    currency_pair cp11 = string_to_pair(str1, "/");
    EXPECT_TRUE(cp11 == cp1);
    //
    std::string str2 = currency_pair_string(cp2, "-");
    currency_pair cp21 = string_to_pair(str2, "-");
    EXPECT_TRUE(cp21 == cp2);
    //
    std::string str3 = currency_pair_string(cp3, ":");
    currency_pair cp31 = string_to_pair(str3, ":");
    EXPECT_TRUE(cp31 == cp3);
  }
  {
    currency_pair cp1{{currency::bitstamp_trust, "EUR"}, {currency::ripple_trust, "USD"}};
    currency_pair cp2{{currency::ripple_trust, "USD"}, {currency::bitstamp_trust, "EUR"}};
    currency_pair cp3 = reverse_pair(cp1);
    EXPECT_TRUE(cp2 == cp3);
  }
  {
    currency_pair cp1{{currency::bitstamp_trust, "EUR"}, {currency::ripple_trust, "USD"}};
    QString qstr1 = currency_pair_qstring(cp1, "-");
    std::string s1 = qstr1.toLatin1().data();
    EXPECT_TRUE(compare(
        "EUR.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B-USD.rMxCKbEDwqr76QuheSUMdEGf4B9xJ8m5De", s1));
  }
  {
    currency_pair cp1{{currency::bitstamp_trust, "EUR"}, {currency::ripple_trust, "USD"}};
    std::string s1 = currency_pair_lowercase_string(cp1);
    EXPECT_TRUE(compare("eurusd", s1));
  }
}

// ------------------------------------------------------------------
TEST(currency, formatting)
{
  {
    double v = 1.2345678;
    std::string s1 = to_string_with_precision(v, 4);
    EXPECT_TRUE(compare("1.2346", s1));
  }
}

// ------------------------------------------------------------------
TEST(currency, map)
{
  std::vector<currency_pair> pairs{{{"", "USD"}, {"", "XRP"}}, {{"", "USD"}, {"", "BTC"}},
      {{"", "USD"}, {"", "EUR"}}, {{"", "USD"}, {"", "GBP"}}};
  {
    // insert all ppairs into map
    std::map<currency_pair, std::string> test_map;
    for (auto key : pairs) test_map.insert({key, currency_pair_string(key, "/")});
    // check they are present
    for (auto [key, value] : test_map) EXPECT_TRUE(test_map.contains(key));
  }
}

// ------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
