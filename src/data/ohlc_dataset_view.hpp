#pragma once

// STL
#include <mutex>
#include <vector>
// Qt
#include <QVector>
// Qwt
#include "data/ohlctv_sample.hpp"
// Grox
#include "currency/currency.hpp"
#include "data/ohlc_datasets.hpp"
#include "data/ohlc_utils.hpp"
#include "plot/ohlc_chart_data.hpp"

class generic_dataset_view
{
};

/// dataset_view provides functions to access the data array holding a dataset
/// as well as other resampled arrays that hold the same data at lower resolutions.

// ----------------------------------------------------------------------------
class ohlc_dataset_view
{
  private:
  // a map of datasets, key is resolution
  std::map<double, ohlc_datasets*> candles_;

  std::string exchange_;
  currency c1_;
  currency c2_;
  std::string ticker_string_;

  public:
  ohlc_dataset_view(std::string exchange, currency const& c1, currency const& c2);
  ~ohlc_dataset_view();

  void read_from_disk();

  std::vector<double> get_dataset_resolutions();

  void add_dataset(double resolution, ohlc_datasets* new_data)
  {
    candles_[resolution] = new_data;
  }

  // access the underlying data vector
  ohlc_datasets* get_dataset(double resolution) const
  {
    if (candles_.find(resolution) != candles_.end())
      return candles_.at(resolution);
    return nullptr;
  }
  ohlc_chart_data* get_samples()
  {
    return candles_.begin()->second->ohlc_samples_;
  }

  // Add new downloaded data to an existing dataset
  void merge_data(double res, QVector<ohlctv_sample> const& new_ohlc_samples_);

  // access the underlying data vector for live samples
  const ohlc_chart_data* get_live_data() const;
  ohlc_chart_data* get_live_data();
  void delete_live_data_up_to(double msecs);
  // add a new trade sample to build live OHLC candles, returns true when
  // a new candle is started, false when one is (only) updated
  void add_live_data(ohlctv_sample new_sample);
  ohlc_chart_curve* get_live_curve();

  // Get the min/max OHLC values for a given time range
  // Returns the lowest of the lows, and highest of the highs in the OHLC samples
  ohlcv_minmax get_min_max(
    ohlc_chart_data const* dataset, double res, double start_time, double end_time) const;
  ohlcv_minmax get_min_max(double res, double start_time, double end_time) const;

  // Returns the min/max values, expanded by a small % so that scaling of graph
  // axes can adjust to allow a small window on ehter side of the min/max
  ohlcv_minmax get_min_max_window(
    double res, double start_time, double end_time, double percent) const;

  // can be used to repair data by deleting items after date, (then redownloading them)
  void truncate_from_time(double t);

  // Get first/last sample time, value is returned as UTC = unix time stamp * 1000
  double get_last_sample_time(bool include_live);
  double get_first_sample_time();
  double get_time_from_index(std::uint64_t i);

  // compute the average price for a buy at/after time T
  ohlctv_sample get_trade_data_by_volume(double volume, double time, double safety = 10);
  ohlctv_sample get_trade_data_by_value(double dollars, double time, double safety = 10);
  double get_estimated_sell_price(double volume, double time, double safety = 10);
  double get_estimated_buy_price(double volume, double time, double safety = 10);

  std::string const& get_ticker_string()
  {
    return ticker_string_;
  }

  mutable std::mutex live_mutex_;
};
