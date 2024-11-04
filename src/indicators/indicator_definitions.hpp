#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average.hpp"
#include "indicators/moving_average_exponential.hpp"
#include "indicators/moving_average_exponential_volume_weighted.hpp"
#include "indicators/moving_average_volume_weighted.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"
#include "indicators/volatility_bollinger_bands.hpp"
#include "indicators/volatility_garman_klass.hpp"
#include "indicators/volatility_rogers_satchell.hpp"

namespace indicators {

  // Helper to create a typelist
  template <typename... Ts>
  struct typelist;

  // Typelist of all indicator types
  using indicator_typelist = typelist<               // for clang-format
      moving_average,                                //
      moving_average_volume_weighted,                //
      moving_average_exponential,                    //
      moving_average_exponential_volume_weighted,    //
      relative_strength_indicator,                   //
      stochastic_relative_strength_indicator,        //
      volatility_bollinger_bands,                    //
      volatility_garman_klass,                       //
      volatility_rogers_satchell                     //
      >;

  // Generate a variant containing each type from the typelist
  // and a vector with one instance of each type in the typelist
  template <typename T>
  struct types_generator;

  template <typename... Ts>
  struct types_generator<typelist<Ts...>>
  {
    // variant with every type in the typelist
    using type = std::variant<Ts...>;
    // vector containing one of each variant type
    static std::vector<type> generate()
    {
      std::vector<type> temp = {Ts{}...};
      for (auto& v : temp)
      {
        std::visit([](auto& i) { i.init_params(); }, v);
      }
      return temp;
    }
  };

  using indicator_variant = types_generator<indicator_typelist>::type;
  inline std::vector<indicator_variant> available_indicators =
      types_generator<indicator_typelist>::generate();

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
    std::visit([&os](auto const& v) { os << streamer{v}; }, sv.val);
    return os;
  }
  static std::string param_string(param_list const& params)
  {
    std::stringstream stream;
    for (auto const& p : params)
    {
      stream << /*std::get<0>(p) << "," << */ streamer{std::get<1>(p)} << ",";
    }
    return stream.str();
  }

}    // namespace indicators
