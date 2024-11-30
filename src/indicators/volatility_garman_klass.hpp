#pragma once

#include <range/v3/view.hpp>
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average.hpp"

// see https://portfoliooptimizer.io/blog/range-based-volatility-estimators-overview-and-examples-of-usage/#parkinson-volatility-estimator

namespace indicators {
  //----------------------------------------------------------------------------
  struct volatility_garman_klass : public indicator_base
  {
public:
    using result_type = std::vector<float>;

    // ---------------------------------------
    /// Default constructor
    volatility_garman_klass(
        int window_size = 14, ohlc_modes mode = ohlc_modes::low, int num_bands = 1)
      : indicator_base("Garman-Klass", "Garman-Klass volatility", {})
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
      params_ = {                                                             //
          {"Samples", candle_data{ohlc_data_resolutions::minute15, 5000}},    //
          {"Window size", 20},                                                //
          {"scale factor", 1.0},                                              //
          {QString("Num Bands (each 1") + sigma + ")", 1}};
    }

    // ---------------------------------------
    /// override outputs as we produce upper/lower bands
    int num_outputs() const override { return 1 + (2 * num_bands_); }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = std::get<int>(params_[1].value);
      scale_ = std::get<double>(params_[2].value);
      num_bands_ = std::get<int>(params_[3].value);
      buffer1_ = boost::circular_buffer<float>(window_size_);
    }

    // ---------------------------------------
    std::vector<float> operator()(ohlctv_sample const& val)
    {
      // scale according to time resolution of data???
      auto mean = average_(ohlc_mode_extract(ohlc_modes::mid_high_low, val));
      mean = val.close;
      // first part to be summed
      double val1 = 0.5 * std::pow(std::log(val.high / val.low), 2);
      // second part to be summed
      double val2 = (2.0 * std::log(2) - 1) * std::pow(std::log(val.close / val.open), 2);
      buffer1_.push_back(val1 - val2);

      double accum1 = 0;
      for (double elem : buffer1_) { accum1 += elem; }
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
