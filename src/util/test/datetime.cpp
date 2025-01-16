#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
//
#include <QString>
//
#include <fmt/format.h>
//
#include "util/datetime_utils.hpp"
//
#include <gtest/gtest.h>

int main(int argc, char** argv);

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
TEST(util, datetime)
{
  {
    std::uint64_t msec =
        std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    msec = 1736347128670;
    std::string tstr = msecs_unix_to_calendar_time_local(msec);
    EXPECT_TRUE(compare("2025-01-08 15:38:48", tstr));
  }
  {
    std::uint64_t sec =
        std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1);
    sec = 1736347437;
    std::string tstr = secs_unix_to_calendar_time_local(sec);
    EXPECT_TRUE(compare("2025-01-08 15:43:57", tstr));
  }
}

// ------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
