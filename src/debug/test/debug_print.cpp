#include <iostream>
#include <string>
#include "debug/demangle_helper.hpp"

template <typename... T>
bool check_result(std::string expected, T&&... t)
{
  std::cout << debug::print_type<T...>(", ") << std::endl;
  if (bool ok = expected == debug::print_type<T...>(", "))
    return ok;
  else
  {
    std::cout << "expected : " << expected << std::endl;
    return ok;
  }
}

int main(int argc, char** argv)
{
  const char* ptr = "This is a test";
  if (!check_result("char const*", ptr))
    return 1;
  if (!check_result("char const*, char const*", ptr, ptr))
    return 1;
  if (!check_result("void (void*) noexcept", std::free))
    return 1;
  if (!check_result("int (int, char**)", main))
    return 1;
  if (!check_result("<>"))
    return 1;
}
