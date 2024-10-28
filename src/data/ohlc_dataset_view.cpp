#include <algorithm>
#include <cmath>
//
#include <QInputDialog>
//
#include "config/config.hpp"
#include "data/abstract_data_manager.hpp"
#include "data/ohlc_data_exception.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "data/ohlc_utils.hpp"
#include "debug/print.hpp"
#include "util/datetime_utils.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
constexpr int debug_level = 5;
//
template <int Level>
inline constexpr print_threshold<Level, debug_level> man_dbg("DataView");

// ----------------------------------------------------------------------------
ohlc_dataset_view::ohlc_dataset_view(std::string exchange, currency_pair const& cp)
  : exchange_(exchange)
  , ticker_string_(currency_pair_string(cp))
{
  // insert empty highest resolution live dataset
  ohlc_dataset* min_res_live = new ohlc_dataset(ohlc_data_resolutions::minute, ticker_string_);
  live_samples_.insert(std::make_pair(ohlc_data_resolutions::minute, min_res_live));

  // insert empty highest resolution candle dataset
  ohlc_dataset* min_res = new ohlc_dataset(ohlc_data_resolutions::minute, ticker_string_);
  candles_.insert(std::make_pair(ohlc_data_resolutions::minute, min_res));

  // load highest res data from disk
  read_from_disk();

  // generate lower res datasets from loaded data
  auto const& resolutions = ohlc_data_resolutions::available_resolutions();
  for (size_t i = 1; i < resolutions.size(); ++i)
  {
    auto const& res = resolutions[i];
    auto origin_data = get_dataset(res.base_);
    auto new_data = origin_data->downsample(res);
    if (new_data)
    {
      add_dataset(res, new_data);
      origin_data->new_data_subscribers_.subscribe(
          [origin_data, new_data](std::uint64_t) { new_data->downsample_update(origin_data); });
    }
  }
}

// ----------------------------------------------------------------------------
ohlc_dataset_view::~ohlc_dataset_view()
{
  for (auto d : candles_) { delete d.second; }
  candles_.clear();
}

// ----------------------------------------------------------------------------
void ohlc_dataset_view::merge_data(
    double const res, QVector<ohlctv_sample> const& new_ohlc_samples_)
{
  ohlc_dataset* data = get_dataset(res);
  // returns the number of samples that are 'new'
  uint64_t update = data->merge_data(new_ohlc_samples_);
  // write new samples to the main datafile
  global_settings.data_manager_->write_file(
      "bitstamp", ticker_string_, data->data(), update, false);
}

// ----------------------------------------------------------------------------
void ohlc_dataset_view::read_from_disk()
{
  try
  {
    global_settings.data_manager_->read_file(
        exchange_, ticker_string_, candles_.begin()->second->data());
  }
  catch (ohlc_data_exception& e)
  {
    // QInputDialog requires int and not int64 unfortunately
    int64_t index = e.index();
    man_dbg<0>.error(str<>("Data integrity error"), ticker_string_, "at index", ffmt<dec9>(index));
    bool ok = false;
    QString label = "First bad index is :" + QString::number(index);
    index = QInputDialog::getInt(nullptr, "Truncate from", label, index, 0, 1 << 30, 1, &ok);
    if (ok)
    {
      double t = get_time_from_index(index);
      truncate_from_time(t);
    }
  }
}

// ----------------------------------------------------------------------------
void ohlc_dataset_view::truncate_from_time(double t)
{
  for (auto k : candles_)
  {
    auto res = k.first;
    auto samples = k.second;
    auto index = samples->sample_index(t);
    samples->data().resize(index);
    man_dbg<0>.debug(str<>("Truncating"), ticker_string_,
        str<3>(ohlc_data_resolutions::get_resolution(res).name_), "at index", ffmt<dec9>(index));
    if (res == ohlc_data_resolutions::minute)
    {
      global_settings.data_manager_->write_impl(
          "bitstamp", ticker_string_, samples->data(), 0, true);
    }
  }
}

// ----------------------------------------------------------------------------
void ohlc_dataset_view::delete_live_data_up_to(double msecs)
{
  std::lock_guard l(live_mutex_);
  ohlc_chart_data* live_samples = get_live_data(ohlc_data_resolutions::minute);
  QVector<ohlctv_sample>& live_data = live_samples->data();
  auto pos = std::remove_if(live_data.begin(), live_data.end(),
      [msecs](ohlctv_sample& ohlc) { return ohlc.time <= msecs; });
  if (pos != live_data.end()) { live_data.erase(pos); }
}

// ----------------------------------------------------------------------------
double ohlc_dataset_view::get_time_from_index(std::uint64_t i)
{
  double t = 0;
  if (!candles_.begin()->second->data().empty()) { t = candles_.begin()->second->sample_time(i); }
  return t;
}

// ----------------------------------------------------------------------------
double ohlc_dataset_view::get_last_sample_time_msec(bool include_live)
{
  double last = 0;
  if (!candles_.begin()->second->data().empty())
  {
    last = candles_.begin()->second->data().back().time;
  }
  else if (include_live)
  {
    std::lock_guard l(live_mutex_);
    ohlc_chart_data* live_samples = get_live_data(ohlc_data_resolutions::minute);
    if (!live_samples->data().empty()) { last = std::max(last, live_samples->data().back().time); }
  }
  return last;
}

// ----------------------------------------------------------------------------
double ohlc_dataset_view::get_first_sample_time()
{
  double first = 0;
  if (!candles_.begin()->second->data().empty())
  {
    first = candles_.begin()->second->data().front().time;
  }
  std::lock_guard l(live_mutex_);
  ohlc_chart_data* live_samples = get_live_data(ohlc_data_resolutions::minute);
  if (!live_samples->data().empty()) { first = std::min(first, live_samples->data().front().time); }
  return first;
}

// ----------------------------------------------------------------------------
ohlcv_minmax ohlc_dataset_view::get_min_max(
    ohlc_chart_data const* dataset, double res, double view_t1, double view_t2) const
{
  if (dataset->data().empty()) return ohlcv_minmax();
  //
  double data_t1 = dataset->data().front().time;
  double data_t2 = dataset->data().back().time;
  //
  double t1 = std::max(view_t1, data_t1);
  double t2 = std::min(std::max(view_t1, view_t2), data_t2);
  size_t sample1 = static_cast<size_t>((t1 - data_t1) / res);
  size_t sample2 = static_cast<size_t>((t2 - data_t1) / res);

  ohlcv_minmax result;
  // if graph is too far right, show last point range, mark flags as invalid
  if (view_t1 > data_t2)
  {
    result = dataset->minmax_limits(sample2, sample2);
    result.valid_ = false;
  }
  // if graph is too far left, show first point range, mark flags as invalid
  else if (view_t2 < data_t1)
  {
    result = dataset->minmax_limits(sample1, sample1);
    result.valid_ = false;
  }
  else { result = dataset->minmax_limits(sample1, sample2); }
  return result;
}

// ----------------------------------------------------------------------------
ohlcv_minmax ohlc_dataset_view::get_min_max(double res, double start_time, double end_time) const
{
  // min max uses the current dataset resolution for main plot
  auto mm1 = get_min_max(get_dataset(res), res, start_time, end_time);

  // live data is always at highest resolution, but if it is out of range, ignore it
  std::lock_guard l(live_mutex_);
  ohlc_chart_data const* live_samples = get_live_data(ohlc_data_resolutions::minute);
  auto mm2 = get_min_max(live_samples, res, start_time, end_time);
  if (mm2.valid_ == false) { return mm1; }
  return mm1.update(mm2);
}

// ----------------------------------------------------------------------------
ohlcv_minmax ohlc_dataset_view::get_min_max_window(
    double res, double start_time, double end_time, double percent) const
{
  ohlcv_minmax result = get_min_max(res, start_time, end_time);
  auto pdiff = (result.max_price_ - result.min_price_);
  auto vdiff = (result.max_volume_ /* min =  zero */);

  if (pdiff > 0)
  {
    result.min_price_ = result.min_price_ - 2.0 * percent * pdiff;
    result.max_price_ = result.max_price_ + percent * pdiff;
  }
  else
  {
    result.min_price_ = 0;
    result.max_price_ = 1;
  }
  if (vdiff > 0)
  {
    result.min_volume_ = 0;
    result.max_volume_ = result.max_volume_ + percent * vdiff;
  }
  else
  {
    result.min_volume_ = 0;
    result.max_volume_ = 1;
  }
  man_dbg<8>.debug(str<>("min_max"), ohlc_data_resolutions::get_resolution(res).name_,
      msecs_unix_to_calendar_time(start_time), "->", msecs_unix_to_calendar_time(end_time), "(",
      result.min_price_, ",", result.max_price_, ")");
  return result;
}

// ----------------------------------------------------------------------------
ohlc_chart_data* ohlc_dataset_view::get_live_data(candle_res res) { return get_live_dataset(res); }

// ----------------------------------------------------------------------------
ohlc_chart_data const* ohlc_dataset_view::get_live_data(candle_res res) const
{
  return get_live_dataset(res);
}

//// ----------------------------------------------------------------------------
//ohlc_chart_curve* ohlc_dataset_view::get_live_curve()
//{
//  ohlc_dataset* temp = get_dataset(ohlc_data_resolutions::minute);
//  return temp->live_curve_;
//}

// ----------------------------------------------------------------------------
void ohlc_dataset_view::add_live_data(ohlctv_sample new_sample)
{
  // snap sample to last minute in which it occured
  new_sample.time =
      ohlc_data_resolutions::minute * std::trunc(new_sample.time / ohlc_data_resolutions::minute);

  std::lock_guard l(live_mutex_);
  ohlc_chart_data* live_samples = get_live_data(ohlc_data_resolutions::minute);
  // if this is the first one, just add it
  if (live_samples->size() == 0)
  {
    live_samples->append(new_sample);
    return;
  }

  // if new sample is part of last candle, update it
  if (live_samples->data().back().time == new_sample.time)
  {
    auto& prev = live_samples->data().back();
    update_ohlctv_sample(prev, new_sample);
  }
  // extend the series with candles to ensure there are no gaps
  // (gaps can cause index computations to be wrong)
  else
  {
    auto prev = live_samples->data().back();
    prev.high = prev.low = prev.open = prev.close;
    prev.volume = 0;
    while (prev.time < new_sample.time)
    {
      prev.time += ohlc_data_resolutions::minute;
      if (prev.time == new_sample.time)
        live_samples->append(new_sample);
      else
        live_samples->append(prev);
    };
  }
}

// ----------------------------------------------------------------------------
std::vector<double> ohlc_dataset_view::get_dataset_resolutions()
{
  std::vector<double> result;
  for (auto k : candles_) { result.push_back(k.first); }
  return result;
}

// ----------------------------------------------------------------------------
ohlctv_sample ohlc_dataset_view::get_trade_data_by_volume(double volume, double time, double safety)
{
  ohlc_chart_data* samples = get_samples();
  auto index = samples->sample_index(time);
  auto const data = samples->data();
  // we use a factor of 10 to play safe, this can be adjusted
  ohlctv_sample ohlc{0, 0, -1, 0, 0, 0};
  while (ohlc.volume < volume * safety && index < data.size())
  {
    // and accumulate data on prices
    update_ohlctv_sample(ohlc, data[index++]);
  }
  return ohlc;
}

// ----------------------------------------------------------------------------
ohlctv_sample ohlc_dataset_view::get_trade_data_by_value(double dollars, double time, double safety)
{
  ohlc_chart_data* samples = get_samples();
  auto index = samples->sample_index(time);
  auto data = samples->data();
  // we use a factor of 10 to play safe, this can be adjusted
  double val_traded = 0;
  ohlctv_sample ohlc{0, 0, -1, 0, 0, 0};
  while (val_traded < dollars * safety && index < data.size())
  {
    // current candle
    ohlctv_sample const& sample = data[index++];
    // get the volume for current candle
    val_traded += sample.volume * (sample.open + sample.close) / 2.0;
    // and accumulate data on prices
    update_ohlctv_sample(ohlc, sample);
  }
  return ohlc;
}

// ----------------------------------------------------------------------------
double ohlc_dataset_view::get_estimated_sell_price(double volume, double time, double safety)
{
  ohlctv_sample ohlc = get_trade_data_by_volume(volume, time, safety);
  // we have created a candle with enough data to sell the volume requested (+safety factor)
  // return a price based on the traded data we accumulated
  if (ohlc.isValid())
  {
    double price = (25.0 * ohlc.high + 75.0 * ohlc.low) / 100.0;
    return price;
  }
  return 0;
}

// ----------------------------------------------------------------------------
double ohlc_dataset_view::get_estimated_buy_price(double dollars, double time, double safety)
{
  ohlctv_sample ohlc = get_trade_data_by_value(dollars, time, safety);
  // we have created a candle with enough data to sell the volume requested (+safety factor)
  // return a price based on the traded data we accumulated
  if (ohlc.isValid())
  {
    double price = (25.0 * ohlc.high + 75.0 * ohlc.low) / 100.0;
    return price;
  }
  return 0;
}
