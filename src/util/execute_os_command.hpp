#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <string>

// ----------------------------------------------------------------------------
inline std::string execute_os_command(const char* cmd)
{
  std::array<char, 1024> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
  {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
  {
    result += buffer.data();
  }
  result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
  return result;
}
