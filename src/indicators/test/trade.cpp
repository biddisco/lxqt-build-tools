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
      param<candle_data>{"Samples", {ohlc_data_resolutions::hour4, 1000}},    // 0
      param<int>{"Window size", 7},                                           // 1
      param<ohlc_modes>{"mode", ohlc_modes::high},                            // 2
      param<double>{"Percentage fee Buy", 0.0},                               // 3
      param<double>{"Percentage fee Sell", 0.0},                              // 4
      param<double>{"Sliding Gap Upper", 0.5 / 100},                          // 5
      param<double>{"Gradient Threshold Upper", 0.0},                         // 6
      param<double>{"Sliding Gap Lower", 0.5 / 100},                          // 7
      param<double>{"Gradient Threshold Lower", 0.0},                         // 8
  };
  alg_.set_params(params);
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  std::string expected =
      "sell 0.30999 xrp 0.00000 value 0.30999 | buy  0.29321 xrp 1.05724 value 0.30999 | sell "
      "0.28832 xrp 0.00000 value 0.30483 | buy  0.27449 xrp 1.11052 value 0.30483 | sell 0.24092 "
      "xrp 0.00000 value 0.26755 | buy  0.27300 xrp 0.98003 value 0.26755 | sell 0.27525 xrp "
      "0.00000 value 0.26975 | buy  0.30398 xrp 0.88740 value 0.26975 | sell 0.26000 xrp 0.00000 "
      "value 0.23072 | buy  0.26700 xrp 0.86414 value 0.23072 | sell 0.24730 xrp 0.00000 value "
      "0.21370 | buy  0.25537 xrp 0.83681 value 0.21370 | sell 0.23263 xrp 0.00000 value 0.19467 | "
      "buy  0.19890 xrp 0.97873 value 0.19467 | sell 0.17681 xrp 0.00000 value 0.17305 | buy  "
      "0.17573 xrp 0.98478 value 0.17305 | sell 0.17723 xrp 0.00000 value 0.17453 | buy  0.17981 "
      "xrp 0.97065 value 0.17453 | sell 0.17719 xrp 0.00000 value 0.17199 | buy  0.19201 xrp "
      "0.89575 value 0.17199 | sell 0.18082 xrp 0.00000 value 0.16197 | buy  0.19423 xrp 0.83390 "
      "value 0.16197 | sell 0.21045 xrp 0.00000 value 0.17550 | buy  0.22336 xrp 0.78572 value "
      "0.17550 | ";
  EXPECT_EQ(expected, trade_loop(alg));
}

//----------------------------------------------------------------------------
TEST(trade, sliding_stop_fee_02)
{
  using namespace indicators;

  // test trade algorithm using fee = 0.2 %
  trade_sell_sliding_stop alg_{};
  indicators::param_list params = {
      param<candle_data>{"Samples", {ohlc_data_resolutions::hour4, 1000}},    // 0
      param<int>{"Window size", 7},                                           // 1
      param<ohlc_modes>{"mode", ohlc_modes::high},                            // 2
      param<double>{"Percentage fee Buy", 0.2},                               // 3
      param<double>{"Percentage fee Sell", 0.2},                              // 4
      param<double>{"Sliding Gap Upper", 0.5 / 100},                          // 5
      param<double>{"Gradient Threshold Upper", 0.0},                         // 6
      param<double>{"Sliding Gap Lower", 0.5 / 100},                          // 7
      param<double>{"Gradient Threshold Lower", 0.0},                         // 8
  };
  alg_.set_params(params);
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  std::string expected =
      "sell 0.30999 xrp 0.00000 value 0.30937 | buy  0.29321 xrp 1.05301 value 0.30875 | sell "
      "0.28832 xrp 0.00000 value 0.30300 | buy  0.27449 xrp 1.10167 value 0.30240 | sell 0.24092 "
      "xrp 0.00000 value 0.26488 | buy  0.27300 xrp 0.96833 value 0.26435 | sell 0.27525 xrp "
      "0.00000 value 0.26600 | buy  0.30398 xrp 0.87330 value 0.26547 | sell 0.26000 xrp 0.00000 "
      "value 0.22660 | buy  0.26700 xrp 0.84701 value 0.22615 | sell 0.24730 xrp 0.00000 value "
      "0.20904 | buy  0.25537 xrp 0.81695 value 0.20862 | sell 0.23263 xrp 0.00000 value 0.18967 | "
      "buy  0.19890 xrp 0.95168 value 0.18929 | sell 0.17681 xrp 0.00000 value 0.16793 | buy  "
      "0.17573 xrp 0.95373 value 0.16760 | sell 0.17723 xrp 0.00000 value 0.16869 | buy  0.17981 "
      "xrp 0.93629 value 0.16835 | sell 0.17719 xrp 0.00000 value 0.16557 | buy  0.19201 xrp "
      "0.86060 value 0.16524 | sell 0.18082 xrp 0.00000 value 0.15530 | buy  0.19423 xrp 0.79797 "
      "value 0.15499 | sell 0.21045 xrp 0.00000 value 0.16760 | buy  0.22336 xrp 0.74886 value "
      "0.16726 | ";
  EXPECT_EQ(expected, trade_loop(alg));
}

//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  init_test_data();
  return RUN_ALL_TESTS();
}
