#pragma once

#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
//
#include "indicators/indicator_types.hpp"

namespace indicators {

  // ----------------------------------------------------------------------------
  template <class T>
  struct streamer
  {
    T const& val;
  };

  // template instantiation helper for constructor type
  template <class T>
  streamer(T) -> streamer<T>;

  template <class T>
  std::ostream& operator<<(std::ostream& os, streamer<T> s)
  {
    os << s.val;
    return os;
  }

  template <class... Ts>
  std::ostream& operator<<(std::ostream& os, streamer<std::variant<Ts...>> sv)
  {
    std::visit([&os](auto const& v) { os << v.name_.toStdString() << " " << streamer{v}; }, sv.val);
    return os;
  }

  static std::string param_string(param_list const& params)
  {
    std::stringstream stream;
    for (auto const& p : params) { stream << /*std::get<0>(p) << "," << */ streamer{p} << ","; }
    return stream.str();
  }

}    // namespace indicators
