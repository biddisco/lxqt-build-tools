#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
//
#include <QSettings>
#include <QStandardPaths>
//
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

std::string data_dir = "/home/biddisco/.local/share/grox";
std::string filename = "grox.hdf5";

using namespace HighFive;
namespace ba = boost::accumulators;

//----------------------------------------------------------------------------
TEST(trade, sliding_Stop)
{
  hdf5_ohlc_manager data_manager;
  data_manager.init(data_dir, filename);
  //
  int const min_samples = 6 * 60 * 24 * 30;          // 6 month of 1 minute candle data
  int const hr4_samples = min_samples / (4 * 60);    // 6 month of 4hr candle data
  //
  global_settings.appDataLocation =
      QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first().toLatin1().data() +
      std::string("/grox");
  global_settings.hdfFileName = "grox.hdf5";
  //
  global_settings.data_manager_ =
      std::dynamic_pointer_cast<abstract_dataset_manager>(std::make_shared<hdf5_ohlc_manager>());
  global_settings.data_manager_->init(global_settings.appDataLocation, global_settings.hdfFileName);
  //
  std::shared_ptr<ohlc_dataset_view> hdf5_ohlc =
      std::make_shared<ohlc_dataset_view>("bitstamp", currency_pair{{"XRP"}, {"USD"}});
  ohlc_dataset* dataset = hdf5_ohlc->get_dataset(ohlc_data_resolutions::hour4);
  QVector<ohlctv_sample> result = dataset->data();
  result.resize(hr4_samples);
  //
  using namespace indicators;
  trade_sell_sliding_stop alg_(3, ohlc_modes::high, 0.005);
  alg_.init_params();
  std::shared_ptr<trade_sell_sliding_stop> alg =
      std::dynamic_pointer_cast<trade_sell_sliding_stop>(alg_.create(&alg_, hdf5_ohlc));

  // display
  int i = 0;
  std::stringstream tmp;
  for (auto ohlc : result)
  {
    indicators::buy_sell_point bsp = alg->operator()(ohlc);
    if (bsp.event_type_ == indicators::buy_sell_event_type::buy)
    {
      tmp << fmt::format("buy {:7.05f} xrp {:7.05f} - ", bsp.value_, bsp.tokens_);
    }
    else if (bsp.event_type_ == indicators::buy_sell_event_type::sell)
    {
      tmp << fmt::format("sell {:7.05f} xrp {:7.05f} - ", bsp.value_, bsp.tokens_);
    }
  }
  std::string expected =
      "sell 0.31764 xrp 0.00000 - buy 0.26563 xrp 1.14624 - sell 0.27468 xrp 0.00000 - buy 0.25911 "
      "xrp 1.03031 - sell 0.30289 xrp 0.00000 - buy 0.28154 xrp 0.97486 - sell 0.28548 xrp 0.00000 "
      "- buy 0.25807 xrp 0.95590 - sell 0.25818 xrp 0.00000 - buy 0.19355 xrp 1.20692 - sell "
      "0.19690 xrp 0.00000 - buy 0.16821 xrp 1.22258 - sell 0.19284 xrp 0.00000 - buy 0.16869 xrp "
      "1.24716 - sell 0.18241 xrp 0.00000 - buy 0.18629 xrp 1.19787 - sell 0.18701 xrp 0.00000 - "
      "buy 0.16033 xrp 1.35324 - sell 0.26867 xrp 0.00000 - buy 0.21226 xrp 1.46019 - sell 0.24924 "
      "xrp 0.00000 - buy 0.22195 xrp 1.46948 - sell 0.22220 xrp 0.00000 - buy 0.18602 xrp 1.68186 "
      "- sell 0.20399 xrp 0.00000 - buy 0.21033 xrp 1.56305 - sell 0.26409 xrp 0.00000 - buy "
      "0.25796 xrp 1.53929 - sell 0.27031 xrp 0.00000 - buy 0.19868 xrp 1.81504 - sell 0.20538 xrp "
      "0.00000 - buy 0.21047 xrp 1.69684 - sell 0.21413 xrp 0.00000 - buy 0.22310 xrp 1.71163 - "
      "sell 0.24791 xrp 0.00000 - buy 0.24952 xrp 1.56452 - ";
  EXPECT_EQ(expected, tmp.str());
}

//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
