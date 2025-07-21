#pragma once

//
#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
//
#include <QVector>
//
#include "currency/currency.hpp"
#include "currency/currency_pair.hpp"
#include "currency/ohlctv_sample.hpp"
#include "data/ohlc_dataset.hpp"
#include "data/ohlc_utils.hpp"
#include "data/timebased_chart_data.hpp"

/// dataset_view provides functions to access the data array holding a dataset
/// as well as other resampled arrays that hold the same data at lower resolutions.

// ----------------------------------------------------------------------------
class ohlc_dataset_view
{
  private:
  // a map of datasets, key is resolution
  std::map<double, ohlc_dataset*> candles_;

  // a map of datasets, key is resolution
  std::map<double, ohlc_dataset*> live_samples_;

  // needed for IO and debug messages
  std::string exchange_;
  std::string ticker_string_;

  public:
  ohlc_dataset_view(std::string abstract_exchange, currency_pair const& cp);
  ~ohlc_dataset_view();

  void read_from_disk();

  std::vector<double> get_dataset_resolutions() const;

  void add_dataset(double resolution, ohlc_dataset* new_data) { candles_[resolution] = new_data; }

  // access the underlying data vector
  ohlc_dataset* get_dataset(double resolution) const
  {
    if (candles_.find(resolution) != candles_.end()) return candles_.at(resolution);
    return nullptr;
  }

  // access the underlying data vector
  ohlc_dataset* get_live_dataset(double resolution) const
  {
    if (live_samples_.find(resolution) != candles_.end()) return live_samples_.at(resolution);
    return nullptr;
  }

  ohlc_chart_data const* get_samples() const { return candles_.begin()->second; }

  // Add new downloaded data to an existing dataset
  void merge_data(double res, QVector<ohlctv_sample> const& new_ohlc_samples_);

  // access the underlying data vector for live samples
  ohlc_chart_data const* get_live_data(candle_res res) const;
  ohlc_chart_data* get_live_data(candle_res res);
  void delete_live_data_up_to(double msecs);
  // add a new trade sample to build live OHLC candles, returns true when
  // a new candle is started, false when one is (only) updated
  void add_live_data(ohlctv_sample new_sample);

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

  // Get first/last sample time, value is returned as UTC msecs = unix time stamp * 1000
  double get_last_sample_time_msec(bool include_live) const;
  double get_first_sample_time() const;
  double get_time_from_index(std::uint64_t i) const;

  // compute the average price for a buy at/after time T
  ohlctv_sample get_trade_data_by_volume(double volume, double time, double safety = 10) const;
  ohlctv_sample get_trade_data_by_value(double dollars, double time, double safety = 10) const;
  double get_estimated_sell_price(double volume, double time, double safety = 10) const;
  double get_estimated_buy_price(double volume, double time, double safety = 10) const;

  std::string_view const get_ticker_string() const { return ticker_string_; }

  // -------------------
  // a dataset may be read by multiple threads at a time, but only written by one
  // so we use a shared lock to protect it from read/write conflicts
  using mutex_type = std::shared_mutex;
  std::shared_lock<mutex_type> take_readonly_lock(char const* msg) const;
  std::unique_lock<mutex_type> take_readwrite_lock(char const* msg) const;
  mutable mutex_type live_mutex_;
};
