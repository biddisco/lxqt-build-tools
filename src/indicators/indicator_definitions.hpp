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

namespace indicators {

  using types =
    std::variant<moving_average, moving_average_volume_weighted, moving_average_exponential>;

  inline std::vector<types> available_indicators = {
    moving_average{}, moving_average_volume_weighted{}, moving_average_exponential{},
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

}    // namespace indicators
