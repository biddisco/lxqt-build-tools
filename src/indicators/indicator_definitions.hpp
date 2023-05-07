#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset_view.hpp"

namespace indicator {

  using param_types = std::variant<double, int, bool, candle_res>;

  // ----------------------------------------------------------------------------
  struct moving_average
  {
    const std::string name = "Moving Average";
    //
    std::vector<std::tuple<std::string, param_types>> params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window length", 15.0),
      std::make_tuple<std::string, param_types>("Weighted", true)};

    void generate(std::shared_ptr<ohlc_dataset_view> data) {}
  };

  using types = std::variant<moving_average>;

  static /*const */ std::vector<types> available_indicators = {
    moving_average{},
    //    {"Heikin Ashi", 1, 0, {}},
    //    {"MA gradient", 1, 0, {}},
    //    {"MA cross",    2, 0, {}},
    //    {"MACD",        1, 3, {12, 26, 9}},
  };
}    // namespace indicator
