#pragma once

// STL
#include <vector>
#include <mutex>
// Qt
#include <QVector>
// Qwt
#include <QwtOHLCSample>
// Grox
#include "src/currency.hpp"
#include "src/data/ohlc_datasets.hpp"
#include "src/plot/ohlc_chart_data.hpp"

class ohlc_dataset_view;

// ----------------------------------------------------------------------------
class ohlc_dataset_manager
{
protected:
    std::string data_dir_;
    std::string file_name_;
    std::mutex  hdf5_mutex_;

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
    void read_hdf5(std::string group, std::string dataname, QVector<QwtOHLCSample> &data);

    // write out data to hdf5
    void write_hdf5(std::string group, std::string dataname, const QVector<QwtOHLCSample>& samples,
        const uint64_t update, bool truncate);

    std::shared_ptr<ohlc_dataset_view> create_dataset_view(std::string exchange, const currency &c1, const currency &c2);
};
