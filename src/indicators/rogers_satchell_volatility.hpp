#pragma once

#include <range/v3/view.hpp>
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  struct rogers_satchell_volatility : indicator_base
  {
    using result_type = std::vector<float>;

    // ---------------------------------------
    // fields required for auto gui generation
    const std::string name = "Rogers-Satchell";
    const std::string description = "Rogers-Satchell volatility (default 14 period)";
    const overlay_type overlay = overlay_type::price;

    const QChar sigma = QChar(0xc3, 0x03);

    param_list params = {
      std::make_tuple<QString, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<QString, param_types>("Window size", 20),
      std::make_tuple<QString, param_types>("scale factor", 1.0),
      std::make_tuple<QString, param_types>(QString("Num Bands (each 1") + sigma + ")", 1)};

    // ---------------------------------------
    // Default constructor
    rogers_satchell_volatility(
      int window_size = 14, ohlc_modes mode = ohlc_modes::low, int num_bands = 1)
      : average_{}
      , buffer1_(window_size)
      , scale_{1.0}
      , num_bands_{num_bands}
      , window_size_(window_size)
    {
    }

    // ---------------------------------------
    int num_outputs() override
    {
      return 1 + (2 * num_bands_);
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      auto resolution_ = std::get<candle_res>(std::get<1>(params[0]));
      window_size_ = std::get<int>(std::get<1>(params[1]));
      scale_ = std::get<double>(std::get<1>(params[2]));
      num_bands_ = std::get<int>(std::get<1>(params[3]));
      buffer1_ = boost::circular_buffer<float>(window_size_);
    }

    // ---------------------------------------
    std::vector<float> operator()(ohlctv_sample const& val)
    {
      // scale according to time resolution of data???
      auto mean = average_(ohlc_mode_extract(ohlc_modes::mid_high_low, val));
      // first part to be summed
      double val1 = std::log(val.high / val.open) * std::log(val.high / val.close) +
        std::log(val.low / val.open) * std::log(val.low / val.close);
      buffer1_.push_back(val1);

      double accum1 = 0;
      for (double elem : buffer1_)
      {
        accum1 += elem;
      }
      accum1 *= 1.0 / buffer1_.size();
      last_sigma_ = scale_ * std::sqrt(accum1);
      //
      std::vector<float> outdata;
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
    inline double getLastResult()
    {
      return band_above_;
    }

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
