#pragma once

// STL
#include <mutex>
#include <vector>
// Qt
#include <QVector>
// Grox
#include "currency/currency.hpp"
#include "currency/ohlctv_sample.hpp"
#include "data/abstract_data_manager.hpp"
#include "data/ohlc_dataset.hpp"

/// dataset_manager is the interface between an array and the (hdf5) file
/// user to hold the underlying dataset on disk.
/// The API has a minimal set of functions to simply read and write.
/// More control of the data is provided by the dataset_view

// ----------------------------------------------------------------------------
class hdf5_ohlc_manager : public abstract_dataset_manager
{
  protected:
  std::string data_dir_;
  std::string file_name_;
  std::mutex hdf5_mutex_;

  public:
  hdf5_ohlc_manager();
  ~hdf5_ohlc_manager() override;

  void init(std::string data_dir, std::string filename) override
  {
    data_dir_ = data_dir;
    file_name_ = data_dir + "/" + filename;
    //
    create_data_dir();
  }

  // Make sure that the initial data dir is present
  void create_data_dir() override;

  // read datasets from hdf5 file
  void read_impl(std::string group, std::string dataname, QVector<ohlctv_sample>& data) override;
  void read_impl(std::string group, std::string dataname, QVector<ohlctv_sample>& data,
      std::uint64_t N) override;

  // write out data to hdf5
  void write_impl(std::string group, std::string dataname, QVector<ohlctv_sample> const& samples,
      const uint64_t update, bool truncate) override;
};
