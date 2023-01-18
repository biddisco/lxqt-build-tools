#include <filesystem>
#include <iostream>
#include <cmath>
//
#include <highfive/H5File.hpp>
//
#include "src/print.hpp"
#include "src/data/ohlc_dataset_manager.hpp"
#include "src/data/ohlc_dataset_view.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> man_dbg("DManager");

// ----------------------------------------------------------------------------
ohlc_dataset_manager::ohlc_dataset_manager()
{
}

// ----------------------------------------------------------------------------
ohlc_dataset_manager::~ohlc_dataset_manager()
{
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::create_data_dir()
{
    namespace fs = std::filesystem;
    if (!fs::exists(data_dir_) && !fs::create_directory(data_dir_))
    {
        throw std::runtime_error("Failed to create dir " + data_dir_);
    }
}

// ----------------------------------------------------------------------------
void hdf5_check(const char *msg, herr_t err)
{
    if (err < 0) {
        throw std::runtime_error(std::string("HDF5 Error")+msg);
    }
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::read_hdf5(std::string group, std::string dataname, QVector<QwtOHLCSample> &data)
{
    using namespace HighFive;
    std::string path = group + "/" + dataname;
    if (std::filesystem::exists(file_name_)) {
        man_dbg<0>.debug(str<>("File Open"), "read_hdf5", file_name_);
        File file(file_name_, File::ReadWrite | File::OpenOrCreate);
        if (file.exist(path)) {
            auto dataset = file.getDataSet(path);
            const uint64_t ohlc_size = sizeof(QwtOHLCSample) / sizeof(double);
            std::size_t N = dataset.getElementCount() / ohlc_size;
            man_dbg<0>.debug(str<>("Dataset Read"), path, "size", dec<9>(N));
            data.resize(N);
            dataset.read<double>(reinterpret_cast<double*>(data.data()));
        }
        else {
            data.clear();
            man_dbg<0>.debug(str<>("Dataset missing"), path);
        }
    }
    else {
        data.clear();
    }
    //
    ohlc_datasets::validate_ohlc(data, ohlc_chart_data::minute);
    man_dbg<0>.debug(str<>("File Close"), "read_hdf5", dec<9>(data.size()));
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::write_hdf5(std::string group, std::string dataname,
    QVector<QwtOHLCSample> const &data,
    const uint64_t update, bool truncate)
{
    int valid = ohlc_datasets::validate_ohlc(data, ohlc_chart_data::minute);
    if (valid!=data.size()) {
        man_dbg<0>.error(str<>("Error"), "Aborting write");
        return;
    }

    using namespace HighFive;
    // size of data as an array of doubles
    const uint64_t ohlc_size = sizeof(QwtOHLCSample) / sizeof(double);
    const uint64_t N = data.size() * ohlc_size;

    std::string path = group + "/" + dataname;
    if (!std::filesystem::exists(file_name_)) {
        man_dbg<0>.debug(str<>("File Create"), "write_hdf5", file_name_, path);
        File file(file_name_, File::ReadWrite | File::OpenOrCreate);
    }

    // open for read/write
    man_dbg<0>.debug(str<>("File Open"), "write_hdf5", file_name_, path);
    File file(file_name_, File::ReadWrite);

    // Create dataset if it does not already exist
    if (!file.exist(path)) {
        // 24*60=1440 60s OHLC candles per day, 6 {t,o,h,l,c,v} entries,
        // so the chunking size ought to be around 1440 * 6 = 8640 elements minimum,
        // a week's data will be 60480 doubles, so a nice binary number size for
        // chunking dimensions will be 65536
        // Use unlimited size so that the data can be extended arbitrarily
        DataSpace dataspace = DataSpace({N, DataSpace::UNLIMITED});
        // Set properties to use chunking
        DataSetCreateProps props;
        props.add(Chunking(std::vector<hsize_t>{65536}));
        // Create the dataset and write data
        man_dbg<0>.debug(str<>("Dataset Create"), path, "size", dec<9>(data.size()));
        DataSet dataset =
            file.createDataSet(path, dataspace, create_datatype<double>(), props);
    }
    // if we are extending an existing dataset
    else if (update > 0)
    {
        man_dbg<5>.debug(str<>("Dataset Extend"), path, dec<9>(update));
        DataSet dataset = file.getDataSet(path);
        // resize along 1 dimmension
        dataset.resize({N});

        // create a new hyperslab selection from old end with size of update
        uint64_t offset = data.size() - update;
        Selection sel = dataset.select({offset * ohlc_size}, {update * ohlc_size});
        // write data from the old endpoint into the new hyperslab
        sel.write_raw(reinterpret_cast<const double*>(&data[offset]));
    }
    // truncating a dataset
    else if (truncate)
    {
        man_dbg<0>.debug(str<>("Dataset Truncate"), path, dec<9>(data.size()));
        DataSet dataset = file.getDataSet(path);
        // resize along 1 dimension
        dataset.resize({N});
    }
    man_dbg<0>.debug(str<>("File Close"), "write_hdf5", path, dec<9>(data.size()));
}

// ----------------------------------------------------------------------------
std::shared_ptr<ohlc_dataset_view>
ohlc_dataset_manager::create_dataset_view(std::string exchange, const currency &c1, const currency &c2)
{
    auto view = std::make_shared<ohlc_dataset_view>(exchange, c1, c2);
    return view;
}
