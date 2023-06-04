#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average.hpp"
#include "indicators/moving_average_exponential.hpp"
#include "indicators/moving_average_volume_weighted.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"

namespace indicators {

  // add each new indicator to types and variant
  using types = std::variant<moving_average, moving_average_volume_weighted,
    moving_average_exponential, relative_strength_indicator,
    /*stochastic_oscillator, */ stochastic_relative_strength_indicator>;

  inline std::vector<types> available_indicators = {
    moving_average{}, moving_average_volume_weighted{}, moving_average_exponential{},
    relative_strength_indicator{},
    /*stochastic_oscillator{}, */ stochastic_relative_strength_indicator{}
    //    {"Heikin Ashi", 1, 0, {}},
    //    {"MA gradient", 1, 0, {}},
    //    {"MA cross",    2, 0, {}},
    //    {"MACD",        1, 3, {12, 26, 9}},
  };

  static std::vector<ohlc_datasets*> get_datasets(
    const param_list& params, std::shared_ptr<ohlc_dataset_view> view)
  {
    std::vector<ohlc_datasets*> result;
    for (const auto& p : params)
    {
      if (const candle_res* c = std::get_if<candle_res>(&std::get<1>(p)))
      {
        result.push_back(view->get_dataset(*c));
      }
    }
    return result;
  }

  template <class T>
  struct streamer
  {
    const T& val;
  };
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
    std::visit([&os](const auto& v) { os << streamer{v}; }, sv.val);
    return os;
  }
  static std::string param_string(const param_list& params)
  {
    std::stringstream stream;
    for (const auto& p : params)
    {
      stream << /*std::get<0>(p) << "," << */ streamer{std::get<1>(p)} << ",";
    }
    return stream.str();
  }

}    // namespace indicators
