#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
// Qt
#include <QSettings>
#include <QStandardPaths>
// extern
#include <boost/accumulators/accumulators.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataSpace.hpp>
#include <highfive/H5File.hpp>
// Grox
#include "config/config.hpp"
#include "currency/currency.hpp"
#include "data/ohlc_dataset.hpp"
#include "data/ohlc_utils.hpp"
#include "indicators/trade_sell_sliding_stop.hpp"
#include "io/hdf5_ohlc_manager.hpp"

//----------------------------------------------------------------------------
using namespace HighFive;
namespace ba = boost::accumulators;

std::shared_ptr<ohlc_dataset_view> hdf5_ohlc;
ohlc_dataset* dataset = nullptr;
QVector<ohlctv_sample> data_4h;
//
int const min_samples = 3 * 60 * 24 * 30;          // 3 months of 1 minute candle data
int const hr4_samples = min_samples / (4 * 60);    // 3 months of 4hr candle data

//----------------------------------------------------------------------------
void init_test_data()
{
  // setup paths used by grox main appplication to store datasets
  global_settings.appDataLocation =
      QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first().toLatin1().data() +
      std::string("/grox");
  global_settings.hdfFileName = "grox.hdf5";
  // create an hddf5 data manager to read datasets
  global_settings.data_manager_ = std::make_shared<hdf5_ohlc_manager>();
  global_settings.data_manager_->init(global_settings.appDataLocation, global_settings.hdfFileName);
  // create a view of a ticker dataset using bitstamp data
  hdf5_ohlc = std::make_shared<ohlc_dataset_view>("bitstamp", currency_pair{{"XRP"}, {"USD"}});
  // get downsampled 4h data
  dataset = hdf5_ohlc->get_dataset(ohlc_data_resolutions::hour4);
  data_4h = dataset->data();
  data_4h.resize(hr4_samples);
}

//----------------------------------------------------------------------------
auto trade_loop = [](std::shared_ptr<indicators::trade_sell_sliding_stop> alg) {
  std::stringstream tmp;
  for (auto ohlc : data_4h)
  {
    indicators::buy_sell_point bsp = alg->operator()(ohlc);
    if (bsp.event_type_ == indicators::buy_sell_event_type::buy)
    {
      tmp << fmt::format(
          "buy  {:7.05f} xrp {:7.05f} value {:7.05f} | ", bsp.price_, bsp.tokens_, bsp.value_);
    }
    else if (bsp.event_type_ == indicators::buy_sell_event_type::sell)
    {
      tmp << fmt::format(
          "sell {:7.05f} xrp {:7.05f} value {:7.05f} | ", bsp.price_, bsp.tokens_, bsp.value_);
    }
  }
  return tmp.str();
};

//----------------------------------------------------------------------------
TEST(trade, sliding_stop_fee_00)
{
  using namespace indicators;

  // test trade algorithm using fee = 0.0 %
  trade_sell_sliding_stop alg_{};
  indicators::param_list params = {
      param<candle_data>{"Samples", {ohlc_data_resolutions::hour4}},    // 0
      param<int>{"Window size", 3},                                     // 1
      param<ohlc_modes>{"mode", ohlc_modes::mid_high_low},              // 2
      param<double>{"Percentage fee Buy", 0.0},                         // 3
      param<double>{"Percentage fee Sell", 0.0},                        // 4
      param<double>{"Sliding Gap Upper %", 22.0},                       // 5
      param<double>{"Sliding Gap Lower %", 0.0},                        // 6
      param<double>{"RSI length multiplier", 10.0},                     // 7
      param<double>{"RSI Upper", 0.6},                                  // 8
      param<double>{"RSI Lower", 0.2},                                  // 9
      param<double>{"Gradient Threshold Upper", 0.0},                   // 10
      param<double>{"Gradient Threshold Lower", 0.1},                   // 11
  };
  alg_.set_params(params);
  alg_.create_outputs(hdf5_ohlc);
  alg_.initialize();
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  std::string expected =
      "sell 0.16402 xrp 0.00000 value 0.13144 | buy  0.16390 xrp 0.81083 value 0.13144 | ";
  EXPECT_EQ(expected, trade_loop(alg));
}

//----------------------------------------------------------------------------
TEST(trade, sliding_stop_fee_02)
{
  using namespace indicators;

  // test trade algorithm using fee = 0.2 %
  trade_sell_sliding_stop alg_{};
  indicators::param_list params = {
      param<candle_data>{"Samples", {ohlc_data_resolutions::hour4}},    // 0
      param<int>{"Window size", 3},                                     // 1
      param<ohlc_modes>{"mode", ohlc_modes::mid_high_low},              // 2
      param<double>{"Percentage fee Buy", 0.2},                         // 3
      param<double>{"Percentage fee Sell", 0.2},                        // 4
      param<double>{"Sliding Gap Upper %", 22.0},                       // 5
      param<double>{"Sliding Gap Lower %", 0.0},                        // 6
      param<double>{"RSI length multiplier", 10.0},                     // 7
      param<double>{"RSI Upper", 0.6},                                  // 8
      param<double>{"RSI Lower", 0.2},                                  // 9
      param<double>{"Gradient Threshold Upper", 0.0},                   // 8
      param<double>{"Gradient Threshold Lower", 0.1},                   // 9
  };
  alg_.set_params(params);
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  std::string expected =
      "sell 0.16402 xrp 0.00000 value 0.13117 | buy  0.16390 xrp 0.80759 value 0.13091 | ";
  EXPECT_EQ(expected, trade_loop(alg));
}

//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  init_test_data();
  return RUN_ALL_TESTS();
}
