#pragma once

// STL
#include <vector>
// Qt
#include <QVector>
// Qwt
#include <QwtOHLCSample>
// Grox
#include "src/data/ohlc_datasets.hpp"
#include "src/plot/ohlc_chart_data.hpp"
// extern
#include "hdf5.h"

// ----------------------------------------------------------------------------
class ohlc_dataset_manager
{
protected:
    std::string data_dir_;
    std::string file_name_;

    // a map of datasets, key is resolution
    std::map<double, ohlc_datasets*> candles_;

public:
    ohlc_dataset_manager();
    ~ohlc_dataset_manager();

    void init(std::string data_dir, std::string filename)
    {
        data_dir_ = data_dir;
        file_name_ = filename;
        //
        create_data_dir();
    };

    // Make sure that the initial data dir is present
    void create_data_dir();

    // read datasets from hdf5 file
    void read_hdf5();
    void read_hdf5(QVector<QwtOHLCSample> &data);

    // write out data to hdf5
    void write_hdf5(const QVector<QwtOHLCSample>& samples,
        const uint64_t update, bool truncate);

    // Add new downloaded data to an existing dataset
    void merge_data(double res,
                    const QVector<QwtOHLCSample>& new_ohlc_samples_);

    // can be used to repair data by deleting items after date, (then redownloading them)
    void truncate_from_time(double t);

    // Get first/last sample time, value is returned as UTC = unix time stamp * 1000
    double get_last_sample_time(bool include_live);
    double get_first_sample_time();

    // Get the min/max OHLC values for a given time range
    // Returns the lowest of the lows, and highest of the highs in the OHLC samples
    ohlcv_minmax get_min_max(ohlc_chart_data const *dataset, double res, double start_time, double end_time) const;
    ohlcv_minmax get_min_max(double res, double start_time, double end_time) const;

    // Returns the min/max values, expanded by a small % so that scaling of graph
    // axes can adjust to allow a small window on ehter side of the min/max
    ohlcv_minmax get_min_max_window(double res, double start_time, double end_time, double percent) const;

    std::vector<double> get_dataset_resolutions();

    // access the underlying data vector
    ohlc_datasets *get_dataset(double resolution) const
    {
        if (candles_.find(resolution)!=candles_.end())
            return candles_.at(resolution);
        return nullptr;
    }

    void add_dataset(double resolution, ohlc_datasets *new_data) {
        candles_[resolution] = new_data;
    }

    ohlc_chart_data *get_samples() { return candles_.begin()->second->ohlc_samples_; }

    // add a new trade sample to build live OHLC candles, returns true when
    // a new candle is started, false when one is (only) updated
    bool add_live_data(QwtOHLCSample new_sample);

    void delete_live_data_before(double msecs);

    ohlc_chart_curve *get_live_curve();

    // access the underlying data vector for live samples
    ohlc_chart_data *get_live_data();

    // compute the average price for a buy at/after time T
    QwtOHLCSample get_trade_data_by_volume(double volume, double time, double safety=10);
    QwtOHLCSample get_trade_data_by_value(double dollars, double time, double safety=10);
    double get_estimated_sell_price(double volume, double time, double safety=10);
    double get_estimated_buy_price(double volume, double time, double safety=10);
};
