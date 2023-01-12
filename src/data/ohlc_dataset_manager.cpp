#include <filesystem>
#include <iostream>
#include <cmath>
//
#include <highfive/H5File.hpp>
//
#include "src/print.hpp"
#include "src/data/ohlc_dataset_manager.hpp"

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
        const QVector<QwtOHLCSample>& new_ohlc_samples_)
{
    ohlc_datasets *data = get_dataset(res);
    // returns the number of samples that are 'new'
    uint64_t update = data->merge_data(new_ohlc_samples_);
    // write new samples to the main datafile
    write_hdf5("bitstamp", "XRP-USD", data->ohlc_samples_->data(), update, false);
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::read_hdf5()
{
    read_hdf5("bitstamp", "XRP-USD", candles_.begin()->second->ohlc_samples_->data());
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
        auto dataset = file.getDataSet(path);
        const uint64_t ohlc_size = sizeof(QwtOHLCSample) / sizeof(double);
        std::size_t N = dataset.getElementCount() / ohlc_size;
        man_dbg<0>.debug(str<>("Dataset Read"), path, "size", dec<9>(N));
        data.resize(N);
        dataset.read<double>(reinterpret_cast<double*>(data.data()));
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
        man_dbg<0>.debug(str<>("File Create"), "write_hdf5", file_name_);
        File file(file_name_, File::ReadWrite | File::OpenOrCreate);
    }

    // open for read/write
    man_dbg<0>.debug(str<>("File Open"), "write_hdf5", file_name_);
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
        man_dbg<0>.debug(str<>("Dataset Truncate"), dec<9>(data.size()));
        DataSet dataset = file.getDataSet(path);
        // resize along 1 dimension
        dataset.resize({N});
    }
    man_dbg<0>.debug(str<>("File Close"), "write_hdf5", dec<9>(data.size()));
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::truncate_from_time(double t)
{
    for (auto k : candles_) {
        auto res = k.first;
        auto samples = k.second->ohlc_samples_;
        auto index = samples->sample_index(t);
        samples->data().resize(index);
        man_dbg<0>.debug(str<>("Truncating"),
                     str<3>(ohlc_chart_data::get_resolution(res).name_)
                     , "at index", index);
        if (res==ohlc_chart_data::minute) {
            write_hdf5("bitstamp", "XRP-USD", samples->data(), 0, true);
        }
    }
}

// ----------------------------------------------------------------------------
void ohlc_dataset_manager::delete_live_data_up_to(double msecs)
{
    ohlc_chart_data *live_samples = candles_.begin()->second->live_samples_;
    QVector<QwtOHLCSample> &live_data = live_samples->data();
    if (live_data.size()>0) {
        auto index = live_samples->sample_index(msecs);
        live_data.erase(live_data.begin(), live_data.begin() + index + 1);
    }
}

// ----------------------------------------------------------------------------
double ohlc_dataset_manager::get_last_sample_time(bool include_live)
{
    double last = 0;
    if (!candles_.begin()->second->ohlc_samples_->data().empty()) {
        last = candles_.begin()->second->ohlc_samples_->data().back().time;
    }
    else if (include_live && !candles_.begin()->second->live_samples_->data().empty()) {
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
ohlcv_minmax ohlc_dataset_manager::get_min_max(ohlc_chart_data const *dataset, double res, double start_time, double end_time) const
{
    if (dataset->data().empty()) return ohlcv_minmax{0,0,0,0,false};
    //
    double init_time = dataset->data().front().time;
    double last_time = dataset->data().back().time;
    //
    start_time = std::max(start_time, init_time);
    end_time   = std::min(end_time, last_time);
    //
    size_t sample1 = static_cast<size_t>((start_time-init_time)/res);
    size_t sample2 = static_cast<size_t>((end_time-init_time)/res);

    ohlcv_minmax result;
    // if graph is too far right, show last point range, mark flags as invalid
    if (start_time>last_time) {
        result = dataset->minmax_limits(sample2, sample2);
        result.valid_ = false;
    }
    // if graph is too far left, show first point range, mark flags as invalid
    else if (end_time<init_time) {
        result = dataset->minmax_limits(sample1, sample1);
        result.valid_ = false;
    }
    else {
        result = dataset->minmax_limits(sample1, sample2);
    }
    return result;
}

// ----------------------------------------------------------------------------
ohlcv_minmax ohlc_dataset_manager::get_min_max(double res, double start_time, double end_time) const
{
    // min max uses the current dataset resolution for main plot
    auto mm1 = get_min_max(get_dataset(res)->ohlc_samples_, res, start_time, end_time);

    // live data is always at highest resolution, but if it is out of range, ignore it
    res = ohlc_chart_data::minute;
    auto mm2 = get_min_max(get_dataset(res)->live_samples_, res, start_time, end_time);
    if (mm2.valid_==false) {
        return mm1;
    }
    return mm1.unite(mm2);
}

// ----------------------------------------------------------------------------
ohlcv_minmax ohlc_dataset_manager::get_min_max_window(double res, double start_time, double end_time, double percent) const
{
    ohlcv_minmax result = get_min_max(res, start_time, end_time);
    auto pdiff = (result.max_price_-result.min_price_);
    auto vdiff = (result.max_volume_ /* min =  zero */);

    if (pdiff>0) {
        result.min_price_ = result.min_price_ - 2.0*percent*pdiff;
        result.max_price_ = result.max_price_ + percent*pdiff;
    }
    else {
        result.min_price_ = 0;
        result.max_price_ = 1;
    }
    if (vdiff>0) {
        result.min_volume_ = 0;
        result.max_volume_ = result.max_volume_ + percent*vdiff;
    }
    else {
        result.min_volume_ = 0;
        result.max_volume_ = 1;
    }
    man_dbg<5>.debug(str<>("min_max"), ohlc_chart_data::get_resolution(res).name_
                     , msecs_unix_to_calendar_time(start_time)
                     , "->", msecs_unix_to_calendar_time(end_time)
                     , "(", result.min_price_, ",", result.max_price_, ")");
    return result;
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
bool ohlc_dataset_manager::add_live_data(QwtOHLCSample new_sample)
{
    // snap sample to last minute in which it occured
    new_sample.time = ohlc_chart_data::minute*std::trunc(new_sample.time/ohlc_chart_data::minute);

    auto live_samples = candles_.begin()->second->live_samples_;
    // if this is the first one, just add it
    if (live_samples->size()==0) {
        live_samples->append(new_sample);
        // return true as new candle is being started
        return true;
    }

    // update existing OHLC candle with new data
    size_t index = live_samples->sample_index(new_sample.time);
    if (index>=live_samples->size()) {
        // if there are gaps between incoming data, fill them with last close
        auto prev = live_samples->data().back();
        prev.high = prev.low = prev.open = prev.close;
        for (size_t s=live_samples->size(); s<=index; ++s) {
            prev.time += ohlc_chart_data::minute;
            live_samples->append(prev);
        }
        // return true as new candle is being started
        return true;
    }

    auto &old_sample = live_samples->data()[index];
    update_QwtOHLCSample(old_sample, new_sample);
    // adding to an existing candle
    return false;
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

// ----------------------------------------------------------------------------
QwtOHLCSample ohlc_dataset_manager::get_trade_data_by_volume(double volume, double time, double safety)
{
    ohlc_chart_data *samples = get_samples();
    auto index = samples->sample_index(time);
    const auto data = samples->data();
    // we use a factor of 10 to play safe, this can be adjusted
    QwtOHLCSample ohlc(-1,-1);
    while (ohlc.volume<volume*safety && index<data.size()) {
        // and accumulate data on prices
        update_QwtOHLCSample(ohlc, data[index++]);
    }
    return ohlc;
}

// ----------------------------------------------------------------------------
QwtOHLCSample ohlc_dataset_manager::get_trade_data_by_value(double dollars, double time, double safety)
{
    ohlc_chart_data *samples = get_samples();
    auto index = samples->sample_index(time);
    auto data = samples->data();
    // we use a factor of 10 to play safe, this can be adjusted
    double val_traded = 0;
    QwtOHLCSample ohlc(-1,-1);
    while (val_traded<dollars*safety && index<data.size()) {
        // current candle
        const QwtOHLCSample &sample = data[index++];
        // get the volume for current candle
        val_traded += sample.volume * (sample.open + sample.close)/2.0;
        // and accumulate data on prices
        update_QwtOHLCSample(ohlc, sample);
    }
    return ohlc;
}

// ----------------------------------------------------------------------------
double ohlc_dataset_manager::get_estimated_sell_price(double volume, double time, double safety)
{
    QwtOHLCSample ohlc = get_trade_data_by_volume(volume, time, safety);
    // we have created a candle with enough data to sell the volume requested (+safety factor)
    // return a price based on the traded data we accumulated
    if (ohlc.isValid()) {
        double price = (25.0*ohlc.high + 75.0*ohlc.low)/100.0;
        return price;
    }
    return 0;
}

// ----------------------------------------------------------------------------
double ohlc_dataset_manager::get_estimated_buy_price(double dollars, double time, double safety)
{
    QwtOHLCSample ohlc = get_trade_data_by_value(dollars, time, safety);
    // we have created a candle with enough data to sell the volume requested (+safety factor)
    // return a price based on the traded data we accumulated
    if (ohlc.isValid()) {
        double price = (25.0*ohlc.high + 75.0*ohlc.low)/100.0;
        return price;
    }
    return 0;
}
