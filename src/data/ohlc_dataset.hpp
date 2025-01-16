#pragma once

#include <cstdint>
#include <string>
//
#include <QVector>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/timebased_chart_data.hpp"
#include "util/pubsub.hpp"

using ohlctv_vector = QVector<ohlctv_sample>;

// ----------------------------------------------------------------------------
struct ohlc_dataset : timebased_chart_data<ohlctv_sample>
{
  // for debugging, show the dataset name
  std::string ticker_str_;

  // when this dataset grows, subscribers will be notified
  grox::PublishSubscribe<std::uint64_t> new_data_subscribers_;

  ohlc_dataset(candle_res res, std::string const& name);
  ~ohlc_dataset();

  // Add new downloaded data to the existing dataset
  std::uint64_t merge_data(ohlctv_vector const& new_ohlc_samples);

  // Checks that all data from time T (if present) has consecutive time stamps.
  // Important when merging new downloaded data with old to ensure no gaps have crept in
  static int64_t validate_ohlc(
      ohlctv_vector const& samples, candle_res res, double time, std::string name);

  // DownSample the current dataset to a lower resolution, it is assumed (without checks) that
  // the lower resolution is an exact multiple of the current one giving a simple N:1 downsizing
  ohlc_dataset* downsample(candle_res res);
  ohlc_dataset* downsample_update(ohlc_dataset* other);
};
