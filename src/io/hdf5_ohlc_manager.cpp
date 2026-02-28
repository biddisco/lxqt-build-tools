#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
//
#include <highfive/H5File.hpp>
//
#include "data/ohlc_data_exception.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "debug/logging.hpp"
#include "io/hdf5_ohlc_manager.hpp"

// ----------------------------------------------------------------------------
static auto man_log = grox::log::create("DManager");

// ----------------------------------------------------------------------------
hdf5_ohlc_manager::hdf5_ohlc_manager() {}

// ----------------------------------------------------------------------------
hdf5_ohlc_manager::~hdf5_ohlc_manager() {}

// ----------------------------------------------------------------------------
void hdf5_ohlc_manager::create_data_dir()
{
  namespace fs = std::filesystem;
  if (!fs::exists(data_dir_) && !fs::create_directory(data_dir_))
  {
    throw std::runtime_error("Failed to create dir " + data_dir_);
  }
}

// ----------------------------------------------------------------------------
void hdf5_check(char const* msg, herr_t err)
{
  if (err < 0) { throw std::runtime_error(std::string("HDF5 Error") + msg); }
}

// ----------------------------------------------------------------------------
void hdf5_ohlc_manager::read_impl(
    std::string group, std::string dataname, QVector<ohlctv_sample>& data, std::uint64_t N)
{
  // we do not currently support multi-threaded file access.
  std::lock_guard lock(hdf5_mutex_);

  using namespace HighFive;
  std::string path = group + "/" + dataname;
  if (std::filesystem::exists(file_name_))
  {
    GROX_LOG_DEBUG(man_log, "{:>20} {} read_hdf5 {}", "file open", path, file_name_);
    File file(file_name_, File::ReadWrite | File::OpenOrCreate);
    if (file.exist(path))
    {
      auto dataset = file.getDataSet(path);
      std::uint64_t const ohlc_size = sizeof(ohlctv_sample) / sizeof(double);
      std::uint64_t Nelem = dataset.getElementCount() / ohlc_size;
      std::uint64_t Nread =
          std::min(Nelem, N >= 0 ? N : std::numeric_limits<std::uint64_t>().max());
      data.resize(Nread);
      GROX_LOG_DEBUG(man_log, "{:>20} {} size {:09d} {:09d}", "dataset read", path, Nread, Nelem);
      std::vector<size_t> offset{0};
      std::vector<size_t> size{Nread * ohlc_size};
      Selection slice = dataset.select(offset, size);
      slice.read_raw<double>(reinterpret_cast<double*>(data.data()), create_datatype<double>());
    }
    else
    {
      data.clear();
      GROX_LOG_DEBUG(man_log, "{:>20} {}", "dataset missing", path);
    }
  }
  else { data.clear(); }
  //
  ohlc_dataset::validate_ohlc(data, ohlc_data_resolutions::minute, 0, dataname);
  GROX_LOG_DEBUG(man_log, "{:>20} {} read_hdf5 {:09d}", "file close", path, data.size());
}

// ----------------------------------------------------------------------------
void hdf5_ohlc_manager::read_impl(
    std::string group, std::string dataname, QVector<ohlctv_sample>& data)
{
  read_impl(group, dataname, data, -1);
}

// ----------------------------------------------------------------------------
void hdf5_ohlc_manager::write_impl(std::string group, std::string dataname,
    QVector<ohlctv_sample> const& data, uint64_t const update, bool truncate)
{
  std::string path = group + "/" + dataname;
  int valid = ohlc_dataset::validate_ohlc(data, ohlc_data_resolutions::minute, 0, dataname);
  if (valid != data.size())
  {
    GROX_LOG_ERROR(man_log, "{:>20} {} Aborting write", "Error", path);
    throw ohlc_data_exception(valid);
    return;
  }

  // we do not support multi-threaded file access yet.
  std::lock_guard lock(hdf5_mutex_);

  using namespace HighFive;
  // size of data as an array of doubles
  uint64_t const ohlc_size = sizeof(ohlctv_sample) / sizeof(double);
  uint64_t const N = data.size() * ohlc_size;

  if (!std::filesystem::exists(file_name_))
  {
    GROX_LOG_DEBUG(man_log, "{:>20} {} write_hdf5 {}", "create", path, file_name_);
    File file(file_name_, File::ReadWrite | File::OpenOrCreate);
  }

  // open for read/write
  GROX_LOG_DEBUG(man_log, "{:>20} {} write_hdf5 {}", "open", path, file_name_);
  File file(file_name_, File::ReadWrite);

  // Create dataset if it does not already exist
  if (!file.exist(path))
  {
    // 24*60=1440 60s OHLC candles per day, 6 {t,o,h,l,c,v} entries,
    // so the chunking size ought to be around 1440 * 6 = 8640 elements minimum,
    // a week's data will be 60480 doubles, so a nice binary number size for
    // chunking dimensions will be 65536
    // Use unlimited size so that the data can be extended arbitrarily
    DataSpace dataspace = DataSpace({N}, {DataSpace::UNLIMITED});
    // Set properties to use chunking
    DataSetCreateProps props;
    props.add(Chunking(std::vector<hsize_t>{65536}));
    // Create the dataset and write data
    GROX_LOG_DEBUG(man_log, "{:>20} {} size {:09d}", "create", path, data.size());
    DataSet dataset = file.createDataSet(path, dataspace, create_datatype<double>(), props);
  }
  // if we are extending an existing dataset
  if (update > 0)
  {
    GROX_LOG_DEBUG(man_log, "{:>20} {} {:09d}", "extend", path, update);
    DataSet dataset = file.getDataSet(path);
    // resize along 1 dimmension
    dataset.resize({N});

    // create a new hyperslab selection from old end with size of update
    uint64_t offset = data.size() - update;
    Selection sel = dataset.select({offset * ohlc_size}, {update * ohlc_size});
    // write data from the old endpoint into the new hyperslab
    sel.write_raw(reinterpret_cast<double const*>(&data[offset]));
  }
  // truncating a dataset
  else if (truncate)
  {
    GROX_LOG_DEBUG(man_log, "{:>20} {} {:09d}", "truncate", path, data.size());
    DataSet dataset = file.getDataSet(path);
    // resize along 1 dimension
    dataset.resize({N});
  }
  GROX_LOG_DEBUG(man_log, "{:>20} {} write_hdf5 {:09d}", "file close", path, data.size());
}
