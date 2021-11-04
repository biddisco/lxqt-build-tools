#pragma once

// STL
#include <vector>
// Qt
#include <QVector>
// Qwt
#include <QwtOHLCSample>
// Grox
#include "src/data/ohlc_dataset.hpp"
#include "src/plot/ohlc_chart_data.hpp"
// extern
#include "hdf5.h"

// ----------------------------------------------------------------------------
class data_holder
{
protected:
    std::string data_dir_;
    std::string file_name_;

    ohlc_dataset candles_;

public:
    data_holder();

    void init(std::string data_dir, std::string filename)
    {
        data_dir_ = data_dir;
        file_name_ = filename;
        //
        create_data_dir();
    };


    // Add new downloaded data to the existing dataset
    void merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples,
        const std::vector<double>& new_ohlc_volumes);

    // Make sure that the initial data dir is present
    void create_data_dir();

    // read datasets from hdf5 file
    void read_hdf5();
    void read_hdf5(QVector<QwtOHLCSample> &data, std::vector<double> &volumes);

    // write out data to hdf5
    void write_hdf5(const QVector<QwtOHLCSample>& samples,
        const std::vector<double>& volume, const uint64_t update = 0);

    // empty : true if size==0, false otherwise
    bool empty();

    // Get first/last sample time, value is returned as UTC = unix time stamp * 1000
    double get_last_sample_time();
    double get_first_sample_time();

    // Get the min max OHLC value for a given time range, min and max
    // are the lowest of the lows, and highest of the highs in the OHLC samples
    QwtInterval get_min_max(ohlc_chart_data const &samples, double start_time, double end_time) const;
    QwtInterval get_min_max(double start_time, double end_time) const;

    // Returns the min/max values, expanded by a small % so that scaling of graph
    // axes can adjust to allow a small window on ehter side of the min/max
    QwtInterval get_min_max_window(double start_time, double end_time, double percent) const;

    // access the underlying data vector
    ohlc_chart_data *get_samples() { return candles_.ohlc_samples; }
    QVector<QwtOHLCSample> const &get_data() { return candles_.ohlc_samples->data(); }

    // add a new trade sample to build live OHLC candles
    void add_live_data(QwtOHLCSample new_sample);

    // access the underlying data vector for live samples
    ohlc_chart_data *get_live_samples() { return candles_.live_samples; }
};
