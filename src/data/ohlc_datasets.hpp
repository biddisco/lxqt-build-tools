#pragma once

// STL
#include <string>
// Qt
#include <QVector>
// Grox
#include "data/ohlc_data_resolutions.hpp"
#include "plot/ohlc_chart_curve.hpp"
#include "plot/ohlc_chart_data.hpp"

// ----------------------------------------------------------------------------
struct ohlc_datasets
{
  // persistent downloaded data
  ohlc_chart_data* ohlc_samples_;
  ohlc_chart_curve* ohlc_curve_;

  // live trade data to be included
  ohlc_chart_data* live_samples_;
  ohlc_chart_curve* live_curve_;

  // for debugging, show the dataset name
  std::string ticker_str_;

  ohlc_datasets(double res, const std::string& name);
  ~ohlc_datasets();

  // Add new downloaded data to the existing dataset
  uint64_t merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples);

  // Checks that all data from time T (if present) has consecutive time stamps.
  // Important when merging new downloaded data with old to ensure no gaps
  // have crept in
  static int64_t validate_ohlc(
    QVector<QwtOHLCSample> const& samples, candle_res res, double time, std::string name);

  // Resample the current dataset to a new resolution, it is assumed (without checks)
  // that the new lower resolution is an exact multiple of the current one
  // giving a simple N:1 downsizing
  ohlc_datasets* resample(candle_res res1, candle_res res2);
  ohlc_datasets* resample_update(candle_res res1, ohlc_datasets* other, candle_res res2);
};
