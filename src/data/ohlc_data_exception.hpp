#pragma once

#include <cstdint>
#include <exception>

class ohlc_data_exception : public std::exception
{
  std::uint64_t index_;

  public:
  ohlc_data_exception(std::uint64_t bad_index)
    : index_(bad_index)
  {
  }
  const char* what() const noexcept override
  {
    return "Data integrity error";
  }
  std::uint64_t index()
  {
    return index_;
  }
};
