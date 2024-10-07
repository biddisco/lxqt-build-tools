// STL
#include <vector>
// Qt
#include <QVector>
// Grox
#include "data/ohlc_data_exception.hpp"
#include "data/ohlc_datasets.hpp"
#include "data/timebased_chart_data.hpp"
#include "debug/print.hpp"
#include "util/datetime_utils.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
constexpr int debug_level = 1;
//
template <int Level>
static print_threshold<Level, debug_level> ohlc_dbg("Datasets");

// ----------------------------------------------------------------------------
ohlc_datasets::ohlc_datasets(candle_res res, std::string const& name)
  : timebased_chart_data<ohlctv_sample>(res)
  , ticker_str_(name)
{
  // we do not destroy these in the destructor because they are given to the
  // plot curve object which deletes them when it is destroyed
  live_samples_ = new ohlc_chart_data(res);
}

// ----------------------------------------------------------------------------
ohlc_datasets::~ohlc_datasets()
{
  // qwt curves, own the samples they plot, so we do not need to delete
  //    ohlc_samples_;
  //    live_samples_;
}

// ----------------------------------------------------------------------------
uint64_t ohlc_datasets::merge_data(ohlctv_vector const& new_ohlc_samples_)
{
  uint64_t update = 0;
  // initial data may be empty, so just copy without merge/update
  if (data().size() == 0)
  {
    data() = new_ohlc_samples_;
    return size();
  }
  else if (!new_ohlc_samples_.empty())
  {
    auto last_existing = data().back().time;
    auto first_new = new_ohlc_samples_.front().time;
    // new samples must start exactly one timestep after old
    int offset = (first_new - last_existing) / ohlc_data_resolutions::minute;
    ohlc_dbg<5>.debug(str<>("merging"), ticker_str_, "new samples offset", ffmt<dec6>(offset));
    if (first_new - last_existing != ohlc_data_resolutions::minute)
    {
      // ohlc_dbg<0>.error(str<>("merging"), ticker_str_, "last_existing", ffmt<dec12>(last_existing),
      //   "first_new", ffmt<dec12>(first_new), "difference", ffmt<dec12>(first_new - last_existing));
      if (first_new - last_existing != ohlc_data_resolutions::minute)
        throw std::runtime_error("Data OHLC time mismatch in merge");
    }
    // add new samples
    ohlc_dbg<5>.debug(
      str<>("merging"), ticker_str_, "new samples", ffmt<dec6>(new_ohlc_samples_.size()));
    data().append(new_ohlc_samples_);
    update += new_ohlc_samples_.size();
  }
  return update;
}

// ----------------------------------------------------------------------------
uint64_t offset_index(double init, double time, double res)
{
  uint64_t i = static_cast<uint64_t>((time - init) / res);
  return std::max(uint64_t(0), i);
}

// ----------------------------------------------------------------------------
int64_t ohlc_datasets::validate_ohlc(
  ohlctv_vector const& samples, candle_res res, double time, std::string name)
{
  if (samples.empty())
    return 0;
  //
  double init_time;
  double origin_time = samples.begin()->time;
  uint64_t init_index = 0;
  if (time == 0)
  {
    init_time = origin_time;
  }
  else
  {
    init_index = offset_index(origin_time, time, res);
    init_time = samples.at(init_index).time;
  }
  ohlc_dbg<6>.debug(str<>("validating"), name, str<3>(res.name_), "from",
    msecs_unix_to_calendar_time(init_time), "index", ffmt<dec9>(init_index));

  for (int64_t index = init_index; index < samples.size(); ++index)
  {
    ohlctv_sample const& s1 = samples.at(index);
    //
    double expected_time = origin_time + (res * index);
    if (expected_time != s1.time)
    {
      ohlc_dbg<0>.error(str<>("validation"), name, str<3>(res.name_), "index", ffmt<dec9>(index),
        "expected", msecs_unix_to_calendar_time(expected_time), "found",
        msecs_unix_to_calendar_time(s1.time));
      throw ohlc_data_exception(index);
    }
  }
  ohlc_dbg<1>.debug(str<>("validated"), name, str<3>(res.name_), "from",
    msecs_unix_to_calendar_time(init_time), "index", ffmt<dec9>(init_index));
  return samples.size();
}

// ----------------------------------------------------------------------------
// resample from res2 to res1
ohlc_datasets* ohlc_datasets::resample(candle_res res1, candle_res res2)
{
  ohlc_datasets* result = new ohlc_datasets(res1, ticker_str_);
  result->resample_update(res1, this, res2);
  return result;
}

// ----------------------------------------------------------------------------
// res1 is resolution of this dataset, res2 is (higher) resolution of other
ohlc_datasets* ohlc_datasets::resample_update(
  candle_res res1, ohlc_datasets* other, candle_res res2)
{
  if (other->data().empty())
    return this;

  // how many of the hi-res candles in the new lower-res candle?
  int subsamples = static_cast<int>(res1 / res2);
  ohlc_dbg<6>.debug(str<>("resample"), ticker_str_, str<3>(res2.name_), "subsamples",
    str<3>(res1.name_), ffmt<dec3>(subsamples));

  // Get the final time-point of this dataset if present -
  // and increment it by 1 hi-res sample to get next start time
  double start_T, orig_T = 0;
  std::uint64_t orig_size = 0;
  if (!data().empty())
  {
    orig_size = size();
    orig_T = data().back().time;
    start_T = orig_T + res2;
  }
  // empty, resample from the first point of the hi-res dataset
  else
  {
    start_T = other->data().front().time;
  }
  // the start time must start an integral candle at the new resolution
  while (static_cast<int>(0.5 + start_T / res2) % subsamples != 0)
  {
    ohlc_dbg<7>.debug(str<>("candle modulus"), ticker_str_,
      ffmt<dec3>(static_cast<int>(0.5 + start_T / res2) % subsamples), "of",
      ffmt<dec3>(subsamples));
    start_T += res2;
  }

  // What index in the high res data maps to selected time start_T
  uint64_t init_sample = other->sample_index(start_T);

  // just exit if there isn't enough hi-res data for a full new resampled candle
  if ((init_sample + subsamples) > other->size())
    return this;

  // we will start a fresh candle from this start_T
  ohlctv_sample current_ohlc = other->data()[init_sample];
  current_ohlc.time = res1 * static_cast<uint64_t>(current_ohlc.time / res1);

  // iterate over all higher res samples for T onwards
  for (ohlctv_vector::const_iterator it = other->data().begin() + init_sample;
       it < other->data().end(); ++it)
  {
    double quantized_time = res1 * static_cast<uint64_t>(it->time / res1);
    int subsample = static_cast<int>(0.5 + it->time / res2) % subsamples;
    // if we are starting a new candle
    if (subsample == 0)
    {
      current_ohlc = *it;
      current_ohlc.time = quantized_time;
    }
    // overwrite the current candle with updated numbers
    else
    {
      update_ohlctv_sample(current_ohlc, *it);
    }
    // finalizing a new candle
    if (subsample == (subsamples - 1))
    {
      append(current_ohlc);
    }
  }
  ohlc_dbg<1>.debug(str<>("resampled"), ticker_str_, str<3>(res1.name_), "from",
    msecs_unix_to_calendar_time(current_ohlc.time), "index", ffmt<dec9>(orig_size), "of", size());
  validate_ohlc(data(), res1, orig_T, ticker_str_);

  return this;
}
