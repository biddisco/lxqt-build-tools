#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
//
#include <boost/process.hpp>
//
namespace bp = boost::process;

// executes the command and returns whatever string is output from the command
// if the command exits with failure, then the return string is empty
// @TODO : improve this to return a bool for fail/success and use a string by ref
static std::string execute_os_command(char const* cmd, bool trim = true)
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
  if (trim) result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
  return result;
}
