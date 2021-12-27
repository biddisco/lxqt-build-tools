#pragma once

// STL
#include <vector>
// Qt
#include <QVector>
// Grox
#include "src/plot/ohlc_chart_data.hpp"
#include "src/plot/ohlc_chart_curve.hpp"

void update_QwtOHLCSample(QwtOHLCSample &ohlc, QwtOHLCSample const &other);
std::string msecs_unix_to_calendar_time(uint64_t unixmsecs);

// ----------------------------------------------------------------------------
struct ohlc_datasets
{
    // persistent downloaded data
    ohlc_chart_data    *ohlc_samples_;
    ohlc_chart_curve   *ohlc_curve_;

    // live trade data to be included
    ohlc_chart_data    *live_samples_;
    ohlc_chart_curve   *live_curve_;

    ohlc_datasets(double res);
    ~ohlc_datasets();

    // Add new downloaded data to the existing dataset
    uint64_t merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples);

    // Checks that all data has consecutive time stamps. Important
    // when merging new downloaded data with old to ensure no gaps
    // have crept in
    static int64_t validate_ohlc(QVector<QwtOHLCSample> const &samples, double res);

    // Resample the current dataset to a new resolution, it is assumed (without checks)
    // that the new lower resolution is an exact multiople of the current one
    // giving a simple N:1 downsizing
    ohlc_datasets *resample(double res1, double res2);
    ohlc_datasets *resample_update(double res1, ohlc_datasets *other, double res2);
};
