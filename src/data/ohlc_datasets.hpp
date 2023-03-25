#pragma once

// STL
#include <optional>
#include <vector>
// Qt
#include <QVector>
// Grox
#include "src/plot/ohlc_chart_data.hpp"
#include "src/plot/ohlc_chart_curve.hpp"

std::string msecs_unix_to_calendar_time(uint64_t unixmsecs);
void update_QwtOHLCSample(QwtOHLCSample &ohlc, QwtOHLCSample const &other);

class ohlc_data_integrity_exception: public std::exception {
    std::uint64_t index_;
public:
    ohlc_data_integrity_exception(std::uint64_t bad_index)
        : index_(bad_index) {
    }
    const char* what() const noexcept override {
        return "Data integrity error";
    }
    std::uint64_t index() { return index_; }
};

struct ohlc_resample
{
    QwtOHLCSample ohlc_;
    //
    ohlc_resample(const QwtOHLCSample &ohlc) : ohlc_(ohlc) {}
    //
    QwtOHLCSample operator()(QwtOHLCSample const &other) {
        update_QwtOHLCSample(ohlc_, other);
        return ohlc_;
    }
};

// ----------------------------------------------------------------------------
struct ohlc_candlemaker
{
    double to_resolution_;
    double from_resolution_;
    QwtOHLCSample ohlc_;
    //
    ohlc_candlemaker(double to_resolution, double from_resolution)
        : to_resolution_(to_resolution)
        , from_resolution_(from_resolution)
        , ohlc_()
    {};
    //
    std::optional<QwtOHLCSample> operator()(const QwtOHLCSample &ohlc) {
        uint64_t candle_old = static_cast<uint64_t>(ohlc_.time / to_resolution_);
        uint64_t candle_cur = static_cast<uint64_t>(ohlc.time / to_resolution_);
        if (candle_old == candle_cur) {
            update_QwtOHLCSample(ohlc_, ohlc);
        }
        else {
            ohlc_ = ohlc;
            ohlc_.time = candle_cur * to_resolution_;
        }
        // is this the last candle before we start a new one
        uint64_t candle_next = static_cast<uint64_t>((ohlc.time + from_resolution_) / to_resolution_);
        if (candle_next > candle_cur) {
            return ohlc_;
        }
        return std::nullopt;
    }

private:
    QwtOHLCSample val_;
};

// ----------------------------------------------------------------------------
struct ohlc_datasets
{
    // persistent downloaded data
    ohlc_chart_data    *ohlc_samples_;
    ohlc_chart_curve   *ohlc_curve_;

    // live trade data to be included
    ohlc_chart_data    *live_samples_;
    ohlc_chart_curve   *live_curve_;

    // for debugging, show the dataset name
    std::string         ticker_str_;

    ohlc_datasets(double res, const std::string &name);
    ~ohlc_datasets();

    // Add new downloaded data to the existing dataset
    uint64_t merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples);

    // Checks that all data from time T (if present) has consecutive time stamps.
    // Important when merging new downloaded data with old to ensure no gaps
    // have crept in
    static int64_t validate_ohlc(QVector<QwtOHLCSample> const &samples, candle_res res, double time, std::string name);

    // Resample the current dataset to a new resolution, it is assumed (without checks)
    // that the new lower resolution is an exact multiople of the current one
    // giving a simple N:1 downsizing
    ohlc_datasets *resample(candle_res res1, candle_res res2);
    ohlc_datasets *resample_update(candle_res res1, ohlc_datasets *other, candle_res res2);
};
