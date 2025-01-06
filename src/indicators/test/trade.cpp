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
int const min_samples = 6 * 60 * 24 * 30;          // 6 month of 1 minute candle data
int const hr4_samples = min_samples / (4 * 60);    // 6 month of 4hr candle data

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
  int i = 0;
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
  trade_sell_sliding_stop alg_(3, ohlc_modes::high, 0.5 / 100, 0.0);
  indicators::param_list params = {                                    //
      {"Samples", candle_data{ohlc_data_resolutions::hour4, 1000}},    // p-0
      {"Window size", 7},                                              // p-1
      {"mode", ohlc_modes::high},                                      // p-2
      {"Sliding Gap", 0.5 / 100},                                      // p-3
      {"Percentage fee", 0.0}};                                        // p-4
  alg_.set_params(params);
  alg_.initialize();
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  std::string expected =
      "sell 0.31764 xrp 0.00000 value 0.29687 | buy  0.26563 xrp 1.14624 value 0.29687 | sell "
      "0.27468 xrp 0.00000 value 0.26811 | buy  0.25911 xrp 1.03031 value 0.26811 | sell 0.30289 "
      "xrp 0.00000 value 0.28076 | buy  0.28154 xrp 0.97486 value 0.28076 | sell 0.28548 xrp "
      "0.00000 value 0.25331 | buy  0.25807 xrp 0.95590 value 0.25331 | sell 0.25818 xrp 0.00000 "
      "value 0.23758 | buy  0.19355 xrp 1.20692 value 0.23758 | sell 0.19690 xrp 0.00000 value "
      "0.21484 | buy  0.16821 xrp 1.22258 value 0.21484 | sell 0.19284 xrp 0.00000 value 0.20940 | "
      "buy  0.16869 xrp 1.24716 value 0.20940 | sell 0.18241 xrp 0.00000 value 0.22340 | buy  "
      "0.18629 xrp 1.19787 value 0.22340 | sell 0.18701 xrp 0.00000 value 0.21558 | buy  0.16033 "
      "xrp 1.35324 value 0.21558 | sell 0.26867 xrp 0.00000 value 0.29154 | buy  0.21226 xrp "
      "1.46019 value 0.29154 | sell 0.24924 xrp 0.00000 value 0.33719 | buy  0.22195 xrp 1.46948 "
      "value 0.33719 | sell 0.22220 xrp 0.00000 value 0.31015 | buy  0.18602 xrp 1.68186 value "
      "0.31015 | sell 0.20399 xrp 0.00000 value 0.33449 | buy  0.21033 xrp 1.56305 value 0.33449 | "
      "sell 0.26409 xrp 0.00000 value 0.40254 | buy  0.25796 xrp 1.53929 value 0.40254 | sell "
      "0.27031 xrp 0.00000 value 0.36480 | buy  0.19868 xrp 1.81504 value 0.36480 | sell 0.20538 "
      "xrp 0.00000 value 0.36295 | buy  0.21047 xrp 1.69684 value 0.36295 | sell 0.21413 xrp "
      "0.00000 value 0.35434 | buy  0.22310 xrp 1.71163 value 0.35434 | sell 0.24791 xrp 0.00000 "
      "value 0.38587 | buy  0.24952 xrp 1.56452 value 0.38587 | ";
  EXPECT_EQ(expected, trade_loop(alg));
}

//----------------------------------------------------------------------------
TEST(trade, sliding_stop_fee_02)
{
  using namespace indicators;

  // test trade algorithm using fee = 0.0 %
  trade_sell_sliding_stop alg_(3, ohlc_modes::high, 0.5 / 100, 0.0);
  indicators::param_list params = {                                    //
      {"Samples", candle_data{ohlc_data_resolutions::hour4, 1000}},    // p-0
      {"Window size", 7},                                              // p-1
      {"mode", ohlc_modes::high},                                      // p-2
      {"Sliding Gap", 0.5 / 100},                                      // p-3
      {"Percentage fee", 0.2}};                                        // p-4
  alg_.set_params(params);
  alg_.initialize();
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  std::string expected =
      "sell 0.31764 xrp 0.00000 value 0.29628 | buy  0.26563 xrp 1.14166 value 0.29569 | sell "
      "0.27468 xrp 0.00000 value 0.26650 | buy  0.25911 xrp 1.02209 value 0.26597 | sell 0.30289 "
      "xrp 0.00000 value 0.27796 | buy  0.28154 xrp 0.96322 value 0.27741 | sell 0.28548 xrp "
      "0.00000 value 0.24978 | buy  0.25807 xrp 0.94072 value 0.24928 | sell 0.25818 xrp 0.00000 "
      "value 0.23334 | buy  0.19355 xrp 1.18300 value 0.23287 | sell 0.19690 xrp 0.00000 value "
      "0.21016 | buy  0.16821 xrp 1.19356 value 0.20974 | sell 0.19284 xrp 0.00000 value 0.20402 | "
      "buy  0.16869 xrp 1.21269 value 0.20361 | sell 0.18241 xrp 0.00000 value 0.21679 | buy  "
      "0.18629 xrp 1.16011 value 0.21636 | sell 0.18701 xrp 0.00000 value 0.20837 | buy  0.16033 "
      "xrp 1.30534 value 0.20795 | sell 0.26867 xrp 0.00000 value 0.28066 | buy  0.21226 xrp "
      "1.40288 value 0.28010 | sell 0.24924 xrp 0.00000 value 0.32330 | buy  0.22195 xrp 1.40616 "
      "value 0.32266 | sell 0.22220 xrp 0.00000 value 0.29619 | buy  0.18602 xrp 1.60296 value "
      "0.29560 | sell 0.20399 xrp 0.00000 value 0.31816 | buy  0.21033 xrp 1.48377 value 0.31753 | "
      "sell 0.26409 xrp 0.00000 value 0.38136 | buy  0.25796 xrp 1.45538 value 0.38060 | sell "
      "0.27031 xrp 0.00000 value 0.34423 | buy  0.19868 xrp 1.70924 value 0.34354 | sell 0.20538 "
      "xrp 0.00000 value 0.34111 | buy  0.21047 xrp 1.59154 value 0.34043 | sell 0.21413 xrp "
      "0.00000 value 0.33169 | buy  0.22310 xrp 1.59900 value 0.33103 | sell 0.24791 xrp 0.00000 "
      "value 0.35976 | buy  0.24952 xrp 1.45573 value 0.35904 | ";
  EXPECT_EQ(expected, trade_loop(alg));
}

//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  init_test_data();
  return RUN_ALL_TESTS();
}
