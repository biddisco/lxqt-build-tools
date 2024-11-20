#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class volatility_bollinger_bands : public indicator_base
  {
public:
    using result_type = std::vector<float>;

    // ---------------------------------------
    /// Default constructor
    volatility_bollinger_bands(
        int window_size = 14, ohlc_modes mode = ohlc_modes::low, int num_bands = 2)
      : indicator_base("Bollinger-Bands", "Bollinger-Bands", {})
      , average_{}
      , num_bands_{num_bands}
      , window_size_(window_size)
      , mode_(mode)
      , buffer_(window_size)
    {
      overlay_ = overlay_vector(1 + (2 * num_bands_), overlay_type::price);
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {//
          std::make_tuple<QString, param_types>(
              "Samples", candle_data{ohlc_data_resolutions::minute15, 5000}),
          std::make_tuple<QString, param_types>("Window size", 14),
          std::make_tuple<QString, param_types>("mode", ohlc_modes::close),
          std::make_tuple<QString, param_types>(QString("Num Bands (each 1") + sigma + ")", 2)};
    }

    // ---------------------------------------
    /// override outputs as we produce upper/lower bands
    int num_outputs() const override { return 1 + (2 * num_bands_); }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = std::get<int>(std::get<1>(params_[1]));
      mode_ = std::get<ohlc_modes>(std::get<1>(params_[2]));
      num_bands_ = std::get<int>(std::get<1>(params_[3]));
      // average_ = moving_average(window_size_, mode_);
      buffer_ = boost::circular_buffer<float>(window_size_);
    }

    // ---------------------------------------
    std::vector<float> operator()(double price)
    {
      auto mean = average_(price);
      buffer_.push_back(price - mean);

      double accum = 0.0;
      std::for_each(
          std::begin(buffer_), std::end(buffer_), [&](double const val) { accum += val * val; });

      // stddev of 'true' mean (not sample mean) uses N-1
      float N = buffer_.size() > 1 ? (buffer_.size() - 1) : 1;
      last_stdev_ = sqrt(accum / N);
      //
      std::vector<float> outdata;
      // push N bands below the mean
      for (int i = 0; i < num_bands_; ++i)
      {
        band_below_ = mean - ((i + 1) * last_stdev_);
        outdata.push_back(band_below_);
      }
      // push the mean
      outdata.push_back(mean);
      // push N bands above the mean
      for (int i = 0; i < num_bands_; ++i)
      {
        band_above_ = mean + ((i + 1) * last_stdev_);
        outdata.push_back(band_above_);
      }
      //
      return outdata;
    }

    // ---------------------------------------
    std::vector<float> operator()(ohlctv_sample const& val)
    {
      double price = ohlc_mode_extract(mode_, val);
      return operator()(price);
    }

    // ---------------------------------------
    inline double getLastResult() { return band_above_; }

private:
    moving_average average_;
    int num_bands_;
    int window_size_;
    ohlc_modes mode_;
    boost::circular_buffer<float> buffer_;
    float last_stdev_;
    float band_above_;
    float band_below_;
  };

}    // namespace indicators
