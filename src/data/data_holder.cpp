#include <filesystem>
#include <iostream>
#include <cmath>
//
#include "src/data/data_holder.hpp"
//
#ifndef DEBUG_ONLY
# define DEBUG_ONLY(x)
# define DEBUG_ALWAYS(x) { \
    std::stringstream temp; temp << x; \
    std::cout << temp.str() << std::endl; }
#endif

// ----------------------------------------------------------------------------
data_holder::data_holder()
{
    // we do not destroy these in the destructor because they are given to the
    // plot curve object which deletes them when it is destroyed
    ohlc_samples = new OHLCData();
    live_samples = new OHLCData();
}

// ----------------------------------------------------------------------------
void data_holder::create_data_dir()
{
    namespace fs = std::filesystem;
    if (!fs::exists(data_dir_))
    {
        if (!fs::create_directory(data_dir_))
        {
            throw std::runtime_error("Failed to create dir " + data_dir_);
        }
    }
}

// ----------------------------------------------------------------------------
void data_holder::validate_ohlc()
{
    using cit = QVector<QwtOHLCSample>::const_iterator;
    cit li = ohlc_samples->data().begin();
    bool valid = true;
    uint64_t index = 0;
    for (cit i = ohlc_samples->data().begin() + 1; i != ohlc_samples->data().end(); ++i)
    {
        uint64_t t1 = static_cast<uint64_t>(li->time);
        uint64_t t2 = static_cast<uint64_t>(i->time);
        if (t2 - t1 != (60 * 1000))
        {
            std::cerr << "Validation error at index " << index << " " << t1 << " and "
                      << t2 << "dataseet truncated " << std::endl;
            valid = false;
            break;
        }
        li = i;
        index++;
    }
    ohlc_samples->data().resize(index + 1);
    ohlc_volumes.resize(index + 1);
}

// ----------------------------------------------------------------------------
void data_holder::merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples,
    const std::vector<double>& new_ohlc_volumes)
{
    uint64_t update = 0;
    if (ohlc_samples->data().size() == 0)
    {
        ohlc_samples->data() = new_ohlc_samples;
        ohlc_volumes = new_ohlc_volumes;
    }
    else if (!new_ohlc_samples.empty())
    {
        auto last_existing = ohlc_samples->data().back().time;
        auto first_new = new_ohlc_samples.front().time;

        DEBUG_ONLY("existing " << static_cast<uint64_t>(last_existing) << " new "
                  << static_cast<uint64_t>(first_new));
        // 1 minute candle OHLC data is stored in msecs
        if (first_new - last_existing == (60 * 1000))
        {
            DEBUG_ONLY("merging data");
            ohlc_samples->data().append(new_ohlc_samples);
            ohlc_volumes.insert(
                ohlc_volumes.end(), new_ohlc_volumes.begin(), new_ohlc_volumes.end());
            if (size_t(ohlc_samples->data().size()) != ohlc_volumes.size())
            {
                throw std::runtime_error("Data merge problem");
            }
            update = new_ohlc_samples.size();
        }
        else
        {
            throw std::runtime_error("Data OHLC time mismatch in merge");
        }
    }
    // write an update to the main datafile
    write_hdf5(ohlc_samples->data(), ohlc_volumes, update);
}

// ----------------------------------------------------------------------------
void data_holder::read_hdf5()
{
    if (std::filesystem::exists(file_name_))
    {
        DEBUG_ALWAYS("Opening: " << file_name_);
        ohlc_samples->data().clear();
        ohlc_volumes.clear();
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
            ohlc_samples->data().resize(N);
            status = H5Dread(dset1, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                ohlc_samples->data().data());
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
            ohlc_volumes.resize(N);
            status = H5Dread(dset2, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                ohlc_volumes.data());

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
void data_holder::write_hdf5(const QVector<QwtOHLCSample>& samples,
    const std::vector<double>& volume, const uint64_t update)
{
    // check data before attempting to write to disk
    //    if (debug_level>0)
    validate_ohlc();
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
        if (update != 0)
        {
            throw std::runtime_error("Cannot extend dataset before it exists");
        }
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

    DEBUG_ONLY("Dataset size: " << ohlc_samples->data().size());
}

// ----------------------------------------------------------------------------
bool data_holder::empty()
{
    return (ohlc_samples->data().size() == 0 || ohlc_volumes.size() == 0);
}

// ----------------------------------------------------------------------------
double data_holder::get_last_sample_time()
{
    double last = 0;
    if (!ohlc_samples->data().empty()) {
        last = ohlc_samples->data().back().time;
    }
    if (!live_samples->data().empty()) {
        last = std::max(last, live_samples->data().back().time);
    }
    return last;
}

// ----------------------------------------------------------------------------
double data_holder::get_first_sample_time()
{
    double first = 0;
    if (!ohlc_samples->data().empty()) {
        first = ohlc_samples->data().front().time;
    }
    if (!live_samples->data().empty()) {
        first = std::min(first, live_samples->data().front().time);
    }
    return first;
}

// ----------------------------------------------------------------------------
QwtInterval data_holder::get_min_max(OHLCData const &samples, double start_time, double end_time) const
{
    if (samples.data().empty()) return QwtInterval();
    double init_time = samples.data().front().time;
    double last_time = samples.data().back().time;
    start_time = std::max(start_time, init_time);
    end_time   = std::min(end_time, last_time);
    size_t sample1 = static_cast<size_t>((start_time-init_time)/(60 * 1000));
    size_t sample2 = static_cast<size_t>((end_time-init_time)/(60 * 1000));
    // if graph is too far right, show last point range
    if (start_time>last_time)
        return samples.minmax_limits(sample2, sample2);
    // if graph is too far left, show first point range
    if (end_time<init_time)
        return samples.minmax_limits(sample1, sample1);
    return samples.minmax_limits(sample1, sample2);
}

// ----------------------------------------------------------------------------
QwtInterval data_holder::get_min_max(double start_time, double end_time) const
{
    auto mm1 = get_min_max(*ohlc_samples, start_time, end_time);
    auto mm2 = get_min_max(*live_samples, start_time, end_time);
    return mm1.unite(mm2);
}

// ----------------------------------------------------------------------------
QwtInterval data_holder::get_min_max_window(double start_time, double end_time, double percent) const
{
    QwtInterval result = get_min_max(start_time, end_time);
    auto diff = result.width();
    if (result.maxValue() == 0.0)
        return {0.0, 0.1};
    if (diff == 0.0)
        diff = result.maxValue()*0.05;
    return {result.minValue() - percent*diff, result.maxValue() + percent*diff};
}

// ----------------------------------------------------------------------------
void data_holder::add_live_data(QwtOHLCSample new_sample)
{
    // snap sample to last minute in which it occured
    new_sample.time = OHLCData::minute*std::trunc(new_sample.time/OHLCData::minute);

    // if this is the first one, just add it
    if (live_samples->data().empty()) {
        live_samples->append(new_sample);
    }
    // update existing OHLC candle with new data
    else {
        double init_time = live_samples->data().front().time;
        size_t index = static_cast<size_t>((new_sample.time-init_time)/OHLCData::minute);
        if (index>=live_samples->size()) {
            // if there are gaps between incoming data, fill them with last close
            auto prev = live_samples->data().back();
            prev.high = prev.low = prev.open = prev.close;
            for (size_t s=live_samples->size(); s<=index; ++s) {
                prev.time += OHLCData::minute;
                live_samples->append(prev);
            }
        }
        auto &old_sample = live_samples->data()[index];
        old_sample.low   = std::min(old_sample.low, new_sample.low);
        old_sample.high  = std::max(old_sample.high, new_sample.high);
        old_sample.close = new_sample.open;
        // if we messed up ...
        assert(old_sample.time == new_sample.time);
    }
}

// ----------------------------------------------------------------------------
