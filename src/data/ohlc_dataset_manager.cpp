#include <filesystem>
#include <iostream>
#include <cmath>
//
#include "src/debug.hpp"
#include "src/data/ohlc_dataset_manager.hpp"

// ----------------------------------------------------------------------------
ohlc_dataset_manager::ohlc_dataset_manager()
{
    ohlc_datasets *min_res = new ohlc_datasets(ohlc_chart_data::minute);
    candles_.insert(std::make_pair(ohlc_chart_data::minute, min_res));
}

// ----------------------------------------------------------------------------
ohlc_dataset_manager::~ohlc_dataset_manager()
{
    for (auto d : candles_) {
        delete d.second;
    }
    candles_.clear();
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
void ohlc_dataset_manager::merge_data(
        const double res,
        const QVector<QwtOHLCSample>& new_ohlc_samples_,
        const std::vector<double>& new_ohlc_volumes_)
{
    ohlc_datasets *data = candles_[res];
    // returns the number of samples that are 'new'
    uint64_t update = data->merge_data(new_ohlc_samples_, new_ohlc_volumes_);
    // write new samples to the main datafile
    write_hdf5(data->ohlc_samples_->data(), data->ohlc_volumes_, update);
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::read_hdf5()
{
    read_hdf5(candles_.begin()->second->ohlc_samples_->data(),
              candles_.begin()->second->ohlc_volumes_);
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::read_hdf5(QVector<QwtOHLCSample> &data, std::vector<double> &volumes)
{
    if (std::filesystem::exists(file_name_))
    {
        DEBUG_ALWAYS("Opening: " << file_name_);
        data.clear();
        volumes.clear();
        //
        hid_t file = H5Fopen(file_name_.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
        // check if datasets exist
        if (H5Lexists(file, "ohlc", H5P_DEFAULT) > 0)
        {
            // read OHLC data
            hid_t dset1 = H5Dopen(file, "ohlc", H5P_DEFAULT);
            hid_t space1 = H5Dget_space(dset1);
            const int ndims1 = H5Sget_simple_extent_ndims(space1);
            hsize_t dims1[ndims1];
            herr_t status = H5Sget_simple_extent_dims(space1, dims1, NULL);
            //
            int N = dims1[0] / (sizeof(QwtOHLCSample) / sizeof(double));
            data.resize(N);
            status = H5Dread(dset1, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                data.data());
            // read Volume data
            hid_t dset2 = H5Dopen(file, "volume", H5P_DEFAULT);
            hid_t space2 = H5Dget_space(dset2);
            const int ndims2 = H5Sget_simple_extent_ndims(space2);
            hsize_t dims2[ndims2];
            status = H5Sget_simple_extent_dims(space2, dims2, NULL);
            if (N != dims2[0])
            {
                throw std::runtime_error("OHLC and Volume datasets not same size");
            }
            volumes.resize(N);
            status = H5Dread(dset2, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                volumes.data());

            // free/close datasets
            status = H5Dclose(dset1);
            status = H5Dclose(dset2);
            // free/close dataspaces
            status = H5Sclose(space1);
            status = H5Sclose(space2);
        }
        // free/close file
        herr_t status = H5Fclose(file);
    }
    else
    {
        DEBUG_ONLY("Creating empty: " << file_name_);

        // Create a new file using default properties.
        hid_t file_id = H5Fcreate(
            file_name_.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        herr_t status = H5Fclose(file_id);
    }
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::write_hdf5(QVector<QwtOHLCSample> const &samples,
    const std::vector<double>& volume, const uint64_t update)
{
    // check data before attempting to write to disk
    //    if (debug_level>0)
    ohlc_datasets::validate_ohlc(samples, ohlc_chart_data::minute);
    //
    DEBUG_ALWAYS("Opening: " << file_name_);

    // In the OHLC dataset:
    // There are 24*60=1440 60s candles per day, and each candle has 5 {t,o,h,l,c} entries,
    // so the chunking size ought to be around 1440 * 5 = 7200 elements minimum,
    // a week's data will be 50400 doubles, so a nice binary number size for
    // chunking dimensions will be 65536
    //
    // The volume dataset has only 1 double per entry so we'll use a chunk dimension
    // 4 times less, of 16384
    //
    // Use unlimited size so that the data can be extended arbitrarily

    const uint64_t ohlc_size = sizeof(QwtOHLCSample) / sizeof(double);
    const uint64_t N = samples.size() * ohlc_size;
    hsize_t ohlc_dims[1] = {N};
    hsize_t vol_dims[1] = {volume.size()};
    hsize_t max_dims[1] = {H5S_UNLIMITED};
    hsize_t chunk_dim1[1] = {65536};
    hsize_t chunk_dim2[1] = {16384};
    herr_t status;

    // open the file, use UNLIMITED for main dimension so we can extend datasets
    hid_t file = H5Fopen(file_name_.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);

    // create datasets if they do not exist already
    if (H5Lexists(file, "ohlc", H5P_DEFAULT) <= 0)
    {
        // create a property list to set the chunking property on our OHLC dataset
        hid_t dprop1 = H5Pcreate(H5P_DATASET_CREATE);
        status = H5Pset_chunk(dprop1, 1, chunk_dim1);

        // write OHLC data,
        hid_t space1 = H5Screate_simple(1, ohlc_dims, max_dims);
        hid_t dset1 = H5Dcreate(
            file, "ohlc", H5T_IEEE_F64LE, space1, H5P_DEFAULT, dprop1, H5P_DEFAULT);
        status = H5Dwrite(
            dset1, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, samples.data());

        // create a property list to set the chunking property on our volume dataset
        hid_t dprop2 = H5Pcreate(H5P_DATASET_CREATE);
        status = H5Pset_chunk(dprop2, 1, chunk_dim2);

        // write Volume data
        hid_t space2 = H5Screate_simple(1, vol_dims, max_dims);
        hid_t dset2 = H5Dcreate(
            file, "volume", H5T_IEEE_F64LE, space2, H5P_DEFAULT, dprop2, H5P_DEFAULT);
        status = H5Dwrite(
            dset2, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, volume.data());
        // free/close datasets
        status = H5Dclose(dset1);
        status = H5Dclose(dset2);
        // free/close properties
        status = H5Pclose(dprop1);
        status = H5Pclose(dprop2);
        // free/close dataspaces
        status = H5Sclose(space1);
        status = H5Sclose(space2);
    }

    // if we are extending a dataset
    else if (update > 0)
    {
        DEBUG_ONLY("Extending datasets by: " << update);
        uint64_t offset = samples.size() - update;
        hsize_t offset1[1] = {offset * ohlc_size};
        hsize_t ext1[1] = {update * ohlc_size};
        hsize_t offset2[1] = {offset};
        hsize_t ext2[1] = {update};

        hid_t dset1 = H5Dopen(file, "ohlc", H5P_DEFAULT);
        // extend dataset to new size
        status = H5Dextend(dset1, ohlc_dims);
        // Select a hyperslab from the file dataspace
        hid_t fspace1 = H5Dget_space(dset1);
        // select hyperslab in new dataset : start, stride(NULL), count, block(NULL)
        status = H5Sselect_hyperslab(fspace1, H5S_SELECT_SET, offset1, NULL, ext1, NULL);
        // Define memory space that we write our new data from
        hid_t dspace1 = H5Screate_simple(1, ext1, NULL);
        // Write new data to the hyperslab
        status = H5Dwrite(
            dset1, H5T_NATIVE_DOUBLE, dspace1, fspace1, H5P_DEFAULT, &samples[offset]);

        hid_t dset2 = H5Dopen(file, "volume", H5P_DEFAULT);
        // extend dataset to new size
        status = H5Dextend(dset2, vol_dims);
        // Select a hyperslab from the file dataspace
        hid_t fspace2 = H5Dget_space(dset2);
        // select hyperslab in new dataset : start, stride(NULL), count, block(NULL)
        status = H5Sselect_hyperslab(fspace2, H5S_SELECT_SET, offset2, NULL, ext2, NULL);
        // Define memory space that we write our new data from
        hid_t dspace2 = H5Screate_simple(1, ext2, NULL);
        // Write new data to the hyperslab
        status = H5Dwrite(
            dset2, H5T_NATIVE_DOUBLE, dspace2, fspace2, H5P_DEFAULT, &volume[offset]);

        // free/close datasets
        status = H5Dclose(dset1);
        status = H5Dclose(dset2);
        // free/close dataspaces
        status = H5Sclose(fspace1);
        status = H5Sclose(fspace2);
        status = H5Sclose(dspace1);
        status = H5Sclose(dspace2);
    }
    // free/close file
    status = H5Fclose(file);

    DEBUG_ONLY("Dataset size: " << data.size());
}

// ----------------------------------------------------------------------------
double ohlc_dataset_manager::get_last_sample_time()
{
    double last = 0;
    if (!candles_.begin()->second->ohlc_samples_->data().empty()) {
        last = candles_.begin()->second->ohlc_samples_->data().back().time;
    }
    if (!candles_.begin()->second->live_samples_->data().empty()) {
        last = std::max(last, candles_.begin()->second->live_samples_->data().back().time);
    }
    return last;
}

// ----------------------------------------------------------------------------
double ohlc_dataset_manager::get_first_sample_time()
{
    double first = 0;
    if (!candles_.begin()->second->ohlc_samples_->data().empty()) {
        first = candles_.begin()->second->ohlc_samples_->data().front().time;
    }
    if (!candles_.begin()->second->live_samples_->data().empty()) {
        first = std::min(first, candles_.begin()->second->live_samples_->data().front().time);
    }
    return first;
}

// ----------------------------------------------------------------------------
QwtInterval ohlc_dataset_manager::get_min_max(ohlc_chart_data const *dataset, double res, double start_time, double end_time) const
{
    if (dataset->data().empty()) return QwtInterval();
    //
    QwtInterval result;
    //
    double init_time = dataset->data().front().time;
    double last_time = dataset->data().back().time;
    //
    start_time = std::max(start_time, init_time);
    end_time   = std::min(end_time, last_time);
    size_t sample1 = static_cast<size_t>((start_time-init_time)/res);
    size_t sample2 = static_cast<size_t>((end_time-init_time)/res);
    // if graph is too far right, show last point range, mark flags as invalid
    if (start_time>last_time) {
        result = dataset->minmax_limits(sample2, sample2);
        result.setBorderFlags(QwtInterval::BorderFlag(255));
    }
    // if graph is too far left, show first point range, mark flags as invalid
    else if (end_time<init_time) {
        result = dataset->minmax_limits(sample1, sample1);
        result.setBorderFlags(QwtInterval::BorderFlag(255));
    }
    else {
        result = dataset->minmax_limits(sample1, sample2);
    }
    return result;
}

// ----------------------------------------------------------------------------
QwtInterval ohlc_dataset_manager::get_min_max(double res, double start_time, double end_time) const
{
    // min max uses the current dataset resolution for main plot
    auto mm1 = get_min_max(get_dataset(res)->ohlc_samples_, res, start_time, end_time);

    // live data is always at highest resolution, but if it is out of range, ignore it
    res = ohlc_chart_data::minute;
    auto mm2 = get_min_max(get_dataset(res)->live_samples_, res, start_time, end_time);
    if (mm2.borderFlags()==QwtInterval::BorderFlag(255)) {
        return mm1;
    }
    return mm1.unite(mm2);
}

// ----------------------------------------------------------------------------
QwtInterval ohlc_dataset_manager::get_min_max_window(double res, double start_time, double end_time, double percent) const
{
    QwtInterval result = get_min_max(res, start_time, end_time);
    auto diff = result.width();
    if (result.maxValue() == 0.0)
        return {0.0, 0.1};
    if (diff == 0.0)
        diff = result.maxValue()*0.05;
    return {result.minValue() - percent*diff, result.maxValue() + percent*diff};
}

// ----------------------------------------------------------------------------
ohlc_chart_data *ohlc_dataset_manager::get_live_data()
{
    ohlc_datasets *temp = get_dataset(ohlc_chart_data::minute);
    return temp->live_samples_;
}

// ----------------------------------------------------------------------------
ohlc_chart_curve *ohlc_dataset_manager::get_live_curve()
{
    ohlc_datasets *temp = get_dataset(ohlc_chart_data::minute);
    return temp->live_curve_;
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::add_live_data(QwtOHLCSample new_sample)
{
    // snap sample to last minute in which it occured
    new_sample.time = ohlc_chart_data::minute*std::trunc(new_sample.time/ohlc_chart_data::minute);

    // if this is the first one, just add it
    if (candles_.begin()->second->live_samples_->data().empty()) {
        candles_.begin()->second->live_samples_->append(new_sample);
    }
    // update existing OHLC candle with new data
    else {
        double init_time = candles_.begin()->second->live_samples_->data().front().time;
        size_t index = static_cast<size_t>((new_sample.time-init_time)/ohlc_chart_data::minute);
        if (index>=candles_.begin()->second->live_samples_->size()) {
            // if there are gaps between incoming data, fill them with last close
            auto prev = candles_.begin()->second->live_samples_->data().back();
            prev.high = prev.low = prev.open = prev.close;
            for (size_t s=candles_.begin()->second->live_samples_->size(); s<=index; ++s) {
                prev.time += ohlc_chart_data::minute;
                candles_.begin()->second->live_samples_->append(prev);
            }
        }
        auto &old_sample = candles_.begin()->second->live_samples_->data()[index];
        old_sample.low   = std::min(old_sample.low, new_sample.low);
        old_sample.high  = std::max(old_sample.high, new_sample.high);
        old_sample.close = new_sample.open;
        // if we messed up ...
        assert(old_sample.time == new_sample.time);
    }
}

// ----------------------------------------------------------------------------
std::vector<double> ohlc_dataset_manager::get_dataset_resolutions()
{
    std::vector<double> result;
    for (auto k : candles_) {
        result.push_back(k.first);
    }
    return result;
}
