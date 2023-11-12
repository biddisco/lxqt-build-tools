#pragma once

#include <QVector>
//
#include <string>
#include <vector>
//
#include "data/ohlctv_sample.hpp"

class abstract_dataset_manager
{
  public:
  abstract_dataset_manager(){};
  virtual ~abstract_dataset_manager(){};
  //
  virtual void init(std::string data_dir, std::string filename){};
  virtual void create_data_dir(){};

  // templated functions to read/write data of type T
  template <typename T>
  void write_file(std::string group, std::string dataname, QVector<T> const& data,
    const uint64_t update, bool truncate)
  {
    this->write_impl(group, dataname, data, update, truncate);
  }

  template <typename T>
  void read_file(std::string group, std::string dataname, QVector<T>& data)
  {
    this->read_impl(group, dataname, data);
  }

  template <typename T>
  void read_file(std::string group, std::string dataname, QVector<T>& data, std::uint64_t N)
  {
    this->read_impl(group, dataname, data, N);
  }

  // virtual functions that implement data loads for different types
  virtual void write_impl(std::string group, std::string dataname,
    QVector<ohlctv_sample> const& data, const uint64_t update, bool truncate){};

  virtual void read_impl(std::string group, std::string dataname, QVector<ohlctv_sample>& data){};
  virtual void read_impl(
    std::string group, std::string dataname, QVector<ohlctv_sample>& data, std::uint64_t N){};
};
