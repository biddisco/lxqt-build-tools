#include <bitset>
#include <iostream>
#include <numeric>
#include <string>
//
#include <fmt/format.h>
//
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"
//
#include <gtest/gtest.h>

int main(int argc, char** argv);

using namespace grox;
using namespace pika::debug::detail;

// ------------------------------------------------------------------
std::string diff(std::string const& s1, std::string const& s2)
{
  std::stringstream tmp;
  int index = 0;
  for (index = 0; index < s1.size() && index < s2.size(); index++)
  {
    if (s1[index] == s2[index])
      tmp << (s1[index] == '\n' ? "\n" : ".");
    else
      tmp << s2[index];
  }
  return tmp.str();
}

// ------------------------------------------------------------------
bool compare(std::string const& expected, std::string const& value)
{
  if (bool ok = (expected == value))
  {
    std::cout << "passed : \"" << expected << "\"" << std::endl;
    return ok;
  }
  else
  {
    std::cout << "expected : \"" << expected << "\" got \"" << value << "\"" << std::endl;
    std::cout << diff(expected, value) << std::endl;
    return ok;
  }
}

// ------------------------------------------------------------------
template <typename... T>
bool test_print_type(std::string expected, T&&... t)
{
  std::stringstream tmp;
  tmp << debug::print_type<T...>(", ");
  return compare(expected, tmp.str());
}

// ------------------------------------------------------------------
TEST(debug_print, print_type)
{
  char const* ptr = "This is a test";
  EXPECT_TRUE(test_print_type("char const*", ptr));
  EXPECT_TRUE(test_print_type("char const*", ptr));
  EXPECT_TRUE(test_print_type("char const*, char const*", ptr, ptr));
  EXPECT_TRUE(test_print_type("void (void*) noexcept", std::free));
  EXPECT_TRUE(test_print_type("int (int, char**)", main));
  EXPECT_TRUE(test_print_type("<>"));
}
// ------------------------------------------------------------------

constexpr static char fp17_15[] = "{:17.15f}";

// ------------------------------------------------------------------
TEST(debug_print, print_format)
{
  {
    std::stringstream tmp;
    tmp << debug::ffmt<dec8>(12345);
    EXPECT_TRUE(compare("00012345", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << fmt::ptr((void*) (0x0000'face));
    EXPECT_TRUE(compare("0x00000000face", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << debug::ffmt<hex12>(0xdead'beef);
    EXPECT_TRUE(compare("0x0000deadbeef", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << debug::ffmt<fp12_8>(3.141592653589793238);
    EXPECT_TRUE(compare("  3.14159265", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << debug::ffmt<fp17_15>(3.141592653589793238);
    EXPECT_TRUE(compare("3.141592653589793", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << debug::ffmt<bin8>(0x01);
    EXPECT_TRUE(compare("00000001", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << debug::ffmt<bin16>(0xfca7);
    EXPECT_TRUE(compare("1111110010100111", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << debug::ffmt<bin8>(0x01) << " " << debug::ffmt<bin16>(0xfca7);
    EXPECT_TRUE(compare("00000001 1111110010100111", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << debug::str<24>("a string of 20 chars");
    EXPECT_TRUE(compare("a string of 20 chars    ", tmp.str()));
  }
  {
    std::stringstream tmp;
    tmp << debug::ipaddr(16885952);
    EXPECT_TRUE(compare("192.168.1.1", tmp.str()));
  }
  {
    //    std::vector<std::uint8_t> buffer(123);
    //    std::iota(buffer.begin(), buffer.end(), 0);
    //    std::stringstream tmp, tmp1;
    //    tmp1 << debug::ptr(buffer.data());
    //    tmp << debug::mem_crc32(buffer.data(), buffer.size(), "Text");
    //    EXPECT_TRUE(compare("Memory: address " + tmp1.str() + " length 0x00007b CRC32:0x8b4999ab\n" +
    //        "0x0706050403020100 0x0f0e0d0c0b0a0908 0x1716151413121110 0x1f1e1d1c1b1a1918\n"
    //        "0x2726252423222120 0x2f2e2d2c2b2a2928 0x3736353433323130 0x3f3e3d3c3b3a3938\n"
    //        "0x4746454443424140 0x4f4e4d4c4b4a4948 0x5756555453525150 0x5f5e5d5c5b5a5958\n"
    //        "0x6766656463626160 0x6f6e6d6c6b6a6968 0x7776757473727170 0x00000000007a7978\n"
    //        ": Text",
    //      tmp.str()));
  }
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
