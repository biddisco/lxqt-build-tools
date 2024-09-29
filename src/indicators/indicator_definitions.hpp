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
  using indicator_typelist = typelist<             // for clang-format
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
      return {Ts{}...};
    }
  };

  using variant_type = types_generator<indicator_typelist>::type;
  inline std::vector<variant_type> available_indicators =
    types_generator<indicator_typelist>::generate();

  // ----------------------------------------------------------------------------
  // iterate over the parameters returned from an indicator selection dialog and
  // find the datasets of the right resolution in the datasets view
  static std::vector<ohlc_datasets*> get_datasets(
    param_list const& params, std::shared_ptr<ohlc_dataset_view> view)
  {
    std::vector<ohlc_datasets*> result;
    for (auto const& p : params)
    {
      if (const candle_res* c = std::get_if<candle_res>(&std::get<1>(p)))
      {
        result.push_back(view->get_dataset(*c));
      }
    }
    return result;
  }

  // ----------------------------------------------------------------------------
  // create a dataset for each indicator output
  template <typename Algorithm>
  std::vector<point_chart_data*>
  create_outputs(const Algorithm& alg, const candle_res res, std::size_t size)
  {
    std::vector<point_chart_data*> output_datasets;
    for (int i = 0; i < alg.num_outputs(); ++i)
    {
      point_chart_data* indicator_data = new point_chart_data(res);
      indicator_data->data().reserve(size);
      output_datasets.push_back(indicator_data);
    }
    return output_datasets;
  }

  // ----------------------------------------------------------------------------
  /// The algorithm might not return a single value, so we provide
  /// overloads that can handle vectors of values
  template <typename Algorithm, typename Datain,
    typename std::enable_if_t<std::is_same<typename Algorithm::result_type, double>::value, bool>
      Enable = false>
  void call_algorithm_operator(
    Algorithm& alg, const Datain& in_data, std::vector<point_chart_data*>& out_datasets)
  {
    for (auto const& ohlc : in_data->data())
    {
      auto vals = alg.operator()(ohlc);
      QPointF xyval(ohlc.time, vals);
      out_datasets[0]->data().push_back(xyval);
    }
  }

  template <typename Algorithm, typename Datain,
    typename std::enable_if_t<
      std::is_same<typename Algorithm::result_type, std::vector<float>>::value, bool>
      Enable = false>
  void call_algorithm_operator(
    Algorithm& alg, const Datain& in_data, std::vector<point_chart_data*>& out_datasets)
  {
    for (auto const& ohlc : in_data->data())
    {
      auto vals = alg.operator()(ohlc);
      for (int i = 0; i < alg.num_outputs(); ++i)
      {
        QPointF xyval(ohlc.time, vals[i]);
        out_datasets[i]->data().push_back(xyval);
      }
    }
  }

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
