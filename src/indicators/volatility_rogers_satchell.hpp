#pragma once

#include <vector>
//
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class volatility_rogers_satchell : public indicator_base
  {
public:
    using operator_type = std::vector<float>;

    // ---------------------------------------
    FACTORY_INDICATOR_V2(volatility_rogers_satchell)

    // ---------------------------------------
    /// Default constructor
    volatility_rogers_satchell(
        int window_size = 14, ohlc_modes mode = ohlc_modes::low, int num_bands = 1)
      : indicator_base("Rogers-Satchell", "Rogers-Satchell volatility", {})
      , average_{}
      , buffer1_(window_size)
      , scale_{1.0}
      , num_bands_{num_bands}
      , window_size_(window_size)
    {
      overlay_ = overlay_vector(1 + (2 * num_bands_), overlay_type::price);
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Window size", 3},                                        // 1
          param<double>{"Scale factor", 1.0},                                  // 2
          param<int>{QString("Num Bands (each 1") + sigma + ")", 1},           // 3
      };
    }

    // ---------------------------------------
    /// override outputs as we produce upper/lower bands
    int num_outputs() const override { return 1 + (2 * num_bands_); }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      scale_ = get<double>(params_, 2);
      num_bands_ = get<int>(params_, 3);
      overlay_ = overlay_vector(1 + (2 * num_bands_), overlay_type::price);
      buffer1_ = boost::circular_buffer<float>(window_size_);
    }

    // ---------------------------------------
    /// Named outputs: "lower_N", "mean", "upper_N" (N = 1..num_bands_)
    output_descriptors get_output_descriptors() const override
    {
      output_descriptors result;
      for (int i = 0; i < num_bands_; ++i)
        result.push_back({"lower_" + std::to_string(i + 1), overlay_type::price});
      result.push_back({"mean", overlay_type::price});
      for (int i = 0; i < num_bands_; ++i)
        result.push_back({"upper_" + std::to_string(i + 1), overlay_type::price});
      return result;
    }

    // ---------------------------------------
    sample_result process_sample(market_sample const& sample) override
    {
      auto const& val = std::get<ohlctv_sample>(sample);
      auto vals = operator()(val);
      output_buffer_ = std::move(vals);
      return std::span<float const>(output_buffer_);
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& val)
    {
      // scale according to time resolution of data???
      auto mean = average_(ohlc_mode_extract(ohlc_modes::mid_high_low, val));
      mean = val.close;
      // first part to be summed
      double val1 =    //
          (std::log(val.high / val.close) * std::log(val.high / val.open)) +
          (std::log(val.low / val.close) * std::log(val.low / val.open));

      buffer1_.push_back(val1);

      double accum1 = 0;
      for (double elem : buffer1_) { accum1 += elem; }
      accum1 *= 1.0 / buffer1_.size();
      last_sigma_ = scale_ * std::sqrt(accum1);
      //
      operator_type outdata;
      // push N bands below the mean
      for (int i = 0; i < num_bands_; ++i)
      {
        band_below_ = mean - ((i + 1) * last_sigma_);
        outdata.push_back(band_below_);
      }
      // push the mean
      outdata.push_back(mean);
      // push N bands above the mean
      for (int i = 0; i < num_bands_; ++i)
      {
        band_above_ = mean + ((i + 1) * last_sigma_);
        outdata.push_back(band_above_);
      }
      //
      return outdata;
    }

    // ---------------------------------------
    inline double getLastResult() { return band_above_; }

private:
    moving_average average_;
    boost::circular_buffer<float> buffer1_;
    double scale_;
    int num_bands_;
    int window_size_;
    float last_sigma_;
    float band_above_;
    float band_below_;
  };

}    // namespace indicators
