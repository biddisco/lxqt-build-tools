#pragma once

#include <list>
#include <memory>
#include <string>
//
#include <QString>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/all.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
//
#include "currency/currency_pair.hpp"
#include "debug/demangle_helper.hpp"
#include "exchange/bitstamp.hpp"
#include "indicators/indicator_ptr.hpp"
#include "indicators/indicator_ref.hpp"
#include "indicators/indicator_registry.hpp"
#include "util/stringutils.hpp"
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_factory.hpp"

namespace indicators {
  // ----------------------------------------------------------------------------
  template <typename T>
  inline nlohmann::ordered_json to_json(indicators::param<T> const& p)
  {
    nlohmann::ordered_json json;
    auto name = p.name_.toStdString();
    json[name]["value"] = fmt::format("{}", p.get());
    return json;
  }

  // ----------------------------------------------------------------------------
  template <>
  inline nlohmann::ordered_json to_json(indicators::param<bool> const& p)
  {
    nlohmann::ordered_json json;
    auto name = p.name_.toStdString();
    json[name]["value"] = p.get();
    return json;
  }

  // ----------------------------------------------------------------------------
  template <>
  inline nlohmann::ordered_json to_json(indicators::param<int> const& p)
  {
    nlohmann::ordered_json json;
    auto name = p.name_.toStdString();
    json[name]["value"] = p.get();
    json[name]["min"] = 0;
    json[name]["max"] = 32768;
    return json;
  }

  // ----------------------------------------------------------------------------
  template <>
  inline nlohmann::ordered_json to_json(indicators::param<double> const& p)
  {
    nlohmann::ordered_json json;
    auto name = p.name_.toStdString();
    json[name]["value"] = p.get();
    json[name]["min"] = -10.0E9;
    json[name]["max"] = 10.0E9;
    return json;
  }

  // ----------------------------------------------------------------------------
  template <>
  inline nlohmann::ordered_json to_json(indicators::param<ohlc_modes> const& p)
  {
    nlohmann::ordered_json json;
    auto name = p.name_.toStdString();
    json[name]["entries"] = ohlc_mode_names;
    json[name]["index"] = int(p.get());
    return json;
  }

  // ----------------------------------------------------------------------------
  template <>
  inline nlohmann::ordered_json to_json(indicators::param<candle_data> const& p)
  {
    nlohmann::ordered_json json;
    auto name = p.name_.toStdString();
    std::vector<std::string> resolutions;
    for (auto const& r : ohlc_data_resolutions::available_resolutions())
    {
      resolutions.push_back(r.name_);
    }
    json[name]["resolutions"] = resolutions;
    json[name]["resolution"] = fmt::format("{}", p.get().res_.name_);
    return json;
  }

  // ----------------------------------------------------------------------------
  template <>
  inline nlohmann::ordered_json to_json(indicators::param<order_book_param> const& p)
  {
    nlohmann::ordered_json json;
    auto name = p.name_.toStdString();
    json[name]["exchanges"] = std::vector<std::string>{p.get().exchange_};
    auto vec = p.get().tickers_ |
        ranges::views::transform([](auto const& c) { return currency_pair_string(c, "-", true); });
    // for (auto const& s : p.get().tickers_) vec.push_back(currency_pair_string(s, "-", true));
    json[name]["tickers"] = fmt::format("{}", vec);
    return json;
  }

  // ----------------------------------------------------------------------------
  template <>
  inline nlohmann::ordered_json to_json(indicators::param<indicator_ref> const& p)
  {
    nlohmann::ordered_json json;
    auto name = p.name_.toStdString();
    json[name]["indicator_name"] = p.get().indicator_name_;
    return json;
  }

}    // namespace indicators

// ----------------------------------------------------------------------------
inline nlohmann::ordered_json get_json_layout_indicator(indicators::shared_algorithm alg)
{
  nlohmann::ordered_json json;
  for (auto const& param : alg->get_params())
  {
    // extract name and type from params and put it into field data
    std::visit(
        [&](auto const& v) {
          using T = std::decay_t<decltype(v.val_)>;
          if constexpr (std::is_same_v<T, indicator_ref>)
          {
            // Emit sub-indicator params as nested JSON layout
            if (v.val_.prototype_)
            {
              nlohmann::ordered_json sub;
              for (auto const& sp : v.val_.prototype_->get_params())
              {
                std::visit(
                    [&](auto const& sv) {
                      using ST = std::decay_t<decltype(sv.val_)>;
                      // Skip candle_data — sub-indicators inherit from parent
                      if constexpr (!std::is_same_v<ST, candle_data>)
                      {
                        sub[sv.name().toStdString()] = grox::debug::print_type<typeof(sv.val_)>();
                      }
                    },
                    sp);
              }
              json[v.name().toStdString()] = sub;
            }
          }
          else
          {
            std::string type = grox::debug::print_type<typeof(v.val_)>();
            json[v.name().toStdString()] = type;
          }
        },
        param);
  }
  return json;
}

// ----------------------------------------------------------------------------
inline nlohmann::json get_json_values_indicator(indicators::shared_algorithm alg)
{
  nlohmann::ordered_json json;
  for (auto const& param : alg->get_params())
  {
    std::visit(
        [&](auto const& v) {
          using T = std::decay_t<decltype(v.val_)>;
          if constexpr (std::is_same_v<T, indicator_ref>)
          {
            // Emit sub-indicator param values as nested JSON
            if (v.val_.prototype_)
            {
              nlohmann::ordered_json sub;
              for (auto const& sp : v.val_.prototype_->get_params())
              {
                std::visit(
                    [&](auto const& sv) {
                      using ST = std::decay_t<decltype(sv.val_)>;
                      if constexpr (!std::is_same_v<ST, candle_data>)
                      {
                        auto sn = sv.name().toStdString();
                        sub[sn] = indicators::to_json(sv)[sn];
                      }
                    },
                    sp);
              }
              json[v.name().toStdString()] = sub;
            }
          }
          else
          {
            auto name = v.name().toStdString();
            json[name] = indicators::to_json(v)[name];
          }
        },
        param);
  }
  return json;
}
