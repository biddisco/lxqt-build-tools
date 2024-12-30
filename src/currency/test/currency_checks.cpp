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
#include "currency/currency_pair.hpp"
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
    currency_code c2{currencies::bitstamp_trust, "USD"};
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
    currency_code c1{currencies::bitstamp_trust, "USD"};
    currency_code c2{currencies::bitstamp_trust, "USD"};
    EXPECT_TRUE(c1 == c2);
  }
  {
    currency_code c1{"", "USD"};
    currency_code c2{currencies::bitstamp_trust, "USD"};
    currency_code c3{"", "XRP"};
    EXPECT_TRUE(c1.is_fiat());
    EXPECT_FALSE(c2.is_fiat());
    EXPECT_TRUE(c3.is_xrp());
  }
  {
    currency_code c1{currencies::bitstamp_trust, "USD"};
    EXPECT_TRUE(compare(c1.to_stringrep(), "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"));
    EXPECT_TRUE(compare(c1.to_stringrep(false), "USD"));
    EXPECT_FALSE(compare(c1.to_stringrep(), "USD"));
    EXPECT_FALSE(compare(c1.to_stringrep(false), "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"));
  }
  {
    currency_code c1{currencies::bitstamp_trust, "USD"};
    EXPECT_TRUE(compare(c1.to_stringrep(), "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"));
    EXPECT_TRUE(compare(c1.to_stringrep(false), "USD"));
    EXPECT_FALSE(compare(c1.to_stringrep(), "USD"));
    EXPECT_FALSE(compare(c1.to_stringrep(false), "USD.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"));
  }
  {
    currency_code c1{"", "USD"};
    currency_code c2{currencies::bitstamp_trust, "USD"};
    currency_code c3{"", "XRP"};
    EXPECT_TRUE(c1.is_fiat());
    EXPECT_FALSE(c2.is_fiat());
    EXPECT_TRUE(c3.is_xrp());
  }
  {
    currency_code c1{"", "USD"};
    std::string str1 = currency_precision(534.123456, c1);
    EXPECT_TRUE(compare(str1, "534.12"));
    //
    currency_code c2{currencies::bitstamp_trust, "USD"};
    std::string str2 = currency_precision(534.123456, c2);
    EXPECT_TRUE(compare(str2, "534.123456"));
  }
}

// ------------------------------------------------------------------
TEST(currency, currency_amount)
{
  {
    currency_amount c1{{"", "USD"}, 0.0};
    currency_amount c2{{"", "USD"}, 0.0};
    EXPECT_TRUE(c1 == c2);
  }
  {
    currency_amount c1{{currencies::bitstamp_trust, "USD"}, 0.0};
    currency_amount c2{{currencies::bitstamp_trust, "USD"}, 0.0};
    EXPECT_TRUE(c1 == c2);
  }
  {
    currency_amount c1{{"", "ABC"}, 0.0};
    currency_amount c2{{"", "BCD"}, 0.0};
    EXPECT_TRUE(c1 < c2);
  }
  {
    currency_amount c1{{"", "USD"}, 0.0};
    currency_amount c2{{currencies::bitstamp_trust, "USD"}, 0.0};
    //
    std::stringstream s1;
    s1 << c1;
    EXPECT_TRUE(compare("USD(0)", s1.str()));
    //
    std::stringstream s2;
    s2 << c2;
    EXPECT_TRUE(compare("USD.bitstamp(0)", s2.str()));
  }
}

// ------------------------------------------------------------------
TEST(currency, currency_pair)
{
  {
    currency_code c1{currencies::bitstamp_trust, "USD"};
    currency_code c2{currencies::bitstamp_trust, "XRP"};
    currency_pair cp1{c1, c2};
    currency_pair cp2{c1, c2};
    EXPECT_TRUE(cp1 == cp2);
  }
  {
    currency_pair cp1{{currencies::bitstamp_trust, "USD"}, {currencies::bitstamp_trust, "XRP"}};
    currency_pair cp2{{currencies::bitstamp_trust, "EUR"}, {currencies::bitstamp_trust, "XRP"}};
    EXPECT_TRUE(cp2 < cp1);
    EXPECT_FALSE(cp2 > cp1);
  }
  {
    currency_pair cp1{{"", "USD"}, {"", "XRP"}};
    currency_pair cp2{{currencies::bitstamp_trust, "USD"}, {"", "XRP"}};
    currency_pair cp3{{currencies::bitstamp_trust, "EUR"}, {currencies::ripple_trust, "USD"}};
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
    currency_pair cp2{{currencies::bitstamp_trust, "USD"}, {"", "XRP"}};
    currency_pair cp3{{currencies::bitstamp_trust, "EUR"}, {currencies::ripple_trust, "USD"}};
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
    currency_pair cp1{{currencies::bitstamp_trust, "EUR"}, {currencies::ripple_trust, "USD"}};
    currency_pair cp2{{currencies::ripple_trust, "USD"}, {currencies::bitstamp_trust, "EUR"}};
    currency_pair cp3 = reverse_pair(cp1);
    EXPECT_TRUE(cp2 == cp3);
  }
  {
    currency_pair cp1{{currencies::bitstamp_trust, "EUR"}, {currencies::ripple_trust, "USD"}};
    QString qstr1 = currency_pair_qstring(cp1, "-");
    std::string s1 = qstr1.toLatin1().data();
    EXPECT_TRUE(compare(
        "EUR.rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B-USD.rMxCKbEDwqr76QuheSUMdEGf4B9xJ8m5De", s1));
  }
  {
    currency_pair cp1{{currencies::bitstamp_trust, "EUR"}, {currencies::ripple_trust, "USD"}};
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
// check comparison operators used in map checks
TEST(currency, map)
{
  std::vector<currency_pair> pairs{
      {{"", "USD"}, {"", "XRP"}},                                                  //
      {{"", "USD"}, {"", "BTC"}},                                                  //
      {{"", "USD"}, {"", "EUR"}},                                                  //
      {{"", "USD"}, {"", "GBP"}},                                                  //
      {{"", "EUR"}, {currencies::ripple_trust, "USD"}},                            //
      {{"", "GBP"}, {currencies::ripple_trust, "USD"}},                            //
      {{"", "BTC"}, {currencies::ripple_trust, "USD"}},                            //
      {{currencies::bitstamp_trust, "GBP"}, {currencies::ripple_trust, "USD"}},    //
      {{currencies::bitstamp_trust, "EUR"}, {currencies::ripple_trust, "USD"}},    //
      {{currencies::ripple_trust, "USD"}, {currencies::gatehub_trust, "EUR"}}      //
  };
  {
    // insert all pairs into map, also add reversed pairs for extra testing
    std::map<currency_pair, std::string> test_map;
    std::vector<currency_pair> pairs2;
    for (auto key : pairs)
    {
      auto rev = reverse_pair(key);
      test_map.insert({key, currency_pair_string(key, "/")});
      test_map.insert({rev, currency_pair_string(rev, "/")});
      pairs2.push_back(key);
      pairs2.push_back(rev);
      for (auto key : pairs)
      {
        currency_pair k2{key}, k3{key}, k4{key}, k5{key};
        k2.c1_.issuer_ += "r";
        EXPECT_FALSE(test_map.contains(k2));
        k3.c2_.issuer_ += "r";
        EXPECT_FALSE(test_map.contains(k3));
        k4.c1_.code_ += "r";
        EXPECT_FALSE(test_map.contains(k4));
        k5.c2_.code_ += "r";
        EXPECT_FALSE(test_map.contains(k5));
      }
    }
    // check they are all detected as present
    for (auto key : pairs2) EXPECT_TRUE(test_map.contains(key));
  }
}

// ------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
