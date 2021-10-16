#pragma once

// Qt
#include <QVector>
// Qwt
#include <QwtOHLCSample>
// STL
#include <vector>
//
#include "hdf5.h"

class data_holder
{
protected:
    std::string data_dir_;
    std::string file_name_;
    //
    QVector<QwtOHLCSample> ohlc_samples;
    std::vector<double> ohlc_volumes;

public:
    void init(std::string data_dir, std::string filename)
    {
        data_dir_ = data_dir;
        file_name_ = filename;
        //
        create_data_dir();
    };
    //
    void validate_ohlc();
    void merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples,
        const std::vector<double>& new_ohlc_volumes);
    //
    void create_data_dir();
    void read_hdf5();
    void write_hdf5(const QVector<QwtOHLCSample>& samples,
        const std::vector<double>& volume, const uint64_t update = 0);
    //
    bool   empty();
    double get_last_sample_time();
    QVector<QwtOHLCSample> const &get_data();
};
