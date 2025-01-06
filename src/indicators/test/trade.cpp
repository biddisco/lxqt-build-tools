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
      {"Samples", candle_data{ohlc_data_resolutions::hour4, 1000}},    // p-0
      {"Window size", 7},                                              // p-1
      {"mode", ohlc_modes::high},                                      // p-2
      {"Percentage fee Buy", 0.0},                                     // p-3
      {"Percentage fee Sell", 0.0},                                    // p-4
      {"Sliding Gap Upper", 0.5 / 100},                                // p-5
      {"Gradient Threshold Upper", 0.0},                               // p-6
      {"Sliding Gap Lower", 0.5 / 100},                                // p-7
      {"Gradient Threshold Lower", 0.0},                               // p-8
  };

  alg_.set_params(params);
  alg_.initialize();
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  std::string expected =
      "sell 0.29899 xrp 0.00000 value 0.29899 | buy  0.29253 xrp 1.02209 value 0.29898 | sell "
      "0.29687 xrp 0.00000 value 0.30343 | buy  0.28500 xrp 1.06468 value 0.30342 | sell 0.23390 "
      "xrp 0.00000 value 0.24903 | buy  0.26022 xrp 0.95700 value 0.24799 | sell 0.27250 xrp "
      "0.00000 value 0.26078 | buy  0.29200 xrp 0.89309 value 0.25999 | sell 0.25984 xrp 0.00000 "
      "value 0.23206 | buy  0.26843 xrp 0.86450 value 0.23182 | sell 0.24854 xrp 0.00000 value "
      "0.21487 | buy  0.25504 xrp 0.84248 value 0.21485 | sell 0.24002 xrp 0.00000 value 0.20221 | "
      "buy  0.19133 xrp 1.05688 value 0.20221 | sell 0.17801 xrp 0.00000 value 0.18813 | buy  "
      "0.17322 xrp 1.08610 value 0.18734 | sell 0.17127 xrp 0.00000 value 0.18602 | buy  0.16740 "
      "xrp 1.11124 value 0.18601 | sell 0.17913 xrp 0.00000 value 0.19906 | buy  0.19448 xrp "
      "1.02353 value 0.19899 | sell 0.17997 xrp 0.00000 value 0.18421 | buy  0.17000 xrp 1.08358 "
      "value 0.18421 | sell 0.21544 xrp 0.00000 value 0.23345 | buy  0.19966 xrp 1.16922 value "
      "0.23345 | ";
  EXPECT_EQ(expected, trade_loop(alg));
}

//----------------------------------------------------------------------------
TEST(trade, sliding_stop_fee_02)
{
  using namespace indicators;

  // test trade algorithm using fee = 0.2 %
  trade_sell_sliding_stop alg_{};
  indicators::param_list params = {
      {"Samples", candle_data{ohlc_data_resolutions::hour4, 1000}},    // p-0
      {"Window size", 7},                                              // p-1
      {"mode", ohlc_modes::high},                                      // p-2
      {"Percentage fee Buy", 0.2},                                     // p-3
      {"Percentage fee Sell", 0.2},                                    // p-4
      {"Sliding Gap Upper", 0.5 / 100},                                // p-5
      {"Gradient Threshold Upper", 0.0},                               // p-6
      {"Sliding Gap Lower", 0.5 / 100},                                // p-7
      {"Gradient Threshold Lower", 0.0},                               // p-8
  };
  alg_.set_params(params);
  alg_.initialize();
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  std::string expected =
      "sell 0.29899 xrp 0.00000 value 0.29839 | buy  0.29253 xrp 1.01801 value 0.29779 | sell "
      "0.29687 xrp 0.00000 value 0.30162 | buy  0.28500 xrp 1.05619 value 0.30100 | sell 0.23390 "
      "xrp 0.00000 value 0.24655 | buy  0.26022 xrp 0.94558 value 0.24503 | sell 0.27250 xrp "
      "0.00000 value 0.25715 | buy  0.29200 xrp 0.87890 value 0.25586 | sell 0.25984 xrp 0.00000 "
      "value 0.22792 | buy  0.26843 xrp 0.84737 value 0.22722 | sell 0.24854 xrp 0.00000 value "
      "0.21019 | buy  0.25504 xrp 0.82248 value 0.20975 | sell 0.24002 xrp 0.00000 value 0.19702 | "
      "buy  0.19133 xrp 1.02767 value 0.19662 | sell 0.17801 xrp 0.00000 value 0.18257 | buy  "
      "0.17322 xrp 1.05186 value 0.18144 | sell 0.17127 xrp 0.00000 value 0.17980 | buy  0.16740 "
      "xrp 1.07191 value 0.17943 | sell 0.17913 xrp 0.00000 value 0.19163 | buy  0.19448 xrp "
      "0.98336 value 0.19118 | sell 0.17997 xrp 0.00000 value 0.17662 | buy  0.17000 xrp 1.03689 "
      "value 0.17627 | sell 0.21544 xrp 0.00000 value 0.22294 | buy  0.19966 xrp 1.11437 value "
      "0.22249 | ";
  EXPECT_EQ(expected, trade_loop(alg));
}

//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  init_test_data();
  return RUN_ALL_TESTS();
}
