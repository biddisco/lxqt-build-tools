#pragma once

// STL
#include <vector>
// Qt
#include <QVector>
// Qwt
#include <QwtOHLCSample>
// Grox
#include "src/plot/ohlc_chart_data.hpp"

// ----------------------------------------------------------------------------
struct ohlc_dataset
{
    // resolution/width of a candlestick
    double resolution_;

    // persistent downloaded data
    ohlc_chart_data    *ohlc_samples;
    std::vector<double> ohlc_volumes;

    // live trade data to be included
    ohlc_chart_data    *live_samples;

    ohlc_dataset(double res);

    double get_resolution() {
        return resolution_;
    }

    // Add new downloaded data to the existing dataset
    uint64_t merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples,
        const std::vector<double>& new_ohlc_volumes);

    // Checks that all data has consecutive time stamps. Important
    // when merging new downloaded data with old to ensure no gaps
    // have crpt in
    static void validate_ohlc(QVector<QwtOHLCSample> const &samples, double res);
};
