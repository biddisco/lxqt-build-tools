#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <string>

static std::string execute_os_command(char const* cmd)
{
  std::array<char, 128> buffer;
  std::string result;
  std::string cmd2 = std::string(cmd) + " 2>&1";    // try to capture stderr too
  auto pipe = popen(cmd2.c_str(), "r");             // get rid of shared_ptr
  if (!pipe) throw std::runtime_error("popen() failed!");
  while (!feof(pipe))
  {
    if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) result += buffer.data();
  }
  auto rc = pclose(pipe);
  if (rc != EXIT_SUCCESS)
  {
    std::cout << "OS Command : " << std::endl
              << cmd2 << std::endl
              << "failed with code " << rc << std::endl
              << result << std::endl;
  }
  result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
  return result;
}
