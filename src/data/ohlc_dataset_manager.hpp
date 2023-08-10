#pragma once

// STL
#include <mutex>
#include <vector>
// Qt
#include <QVector>
// Qwt
#include <QwtOHLCSample>
// Grox
#include "currency.hpp"
#include "data/ohlc_datasets.hpp"

class ohlc_dataset_view;

/// dataset_manager is the interface between an array and the (hdf5) file
/// user to hold the underlying dataset on disk.
/// The API has a minimal set of functions to simply read and write.
/// More control of the data is provided by the dataset_view

// ----------------------------------------------------------------------------
class ohlc_dataset_manager
{
  protected:
  std::string data_dir_;
  std::string file_name_;
  std::mutex hdf5_mutex_;

  public:
  ohlc_dataset_manager();
  ~ohlc_dataset_manager();

  void init(std::string data_dir, std::string filename)
  {
    data_dir_ = data_dir;
    file_name_ = filename;
    //
    create_data_dir();
  }

  // Make sure that the initial data dir is present
  void create_data_dir();

  // read datasets from hdf5 file
  void read_hdf5(std::string group, std::string dataname, QVector<QwtOHLCSample>& data);

  // write out data to hdf5
  void write_hdf5(std::string group, std::string dataname, QVector<QwtOHLCSample> const& samples,
    const uint64_t update, bool truncate);

  std::shared_ptr<ohlc_dataset_view> create_dataset_view(
    std::string exchange, currency const& c1, currency const& c2);
};
