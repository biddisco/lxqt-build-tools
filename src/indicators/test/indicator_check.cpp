/*
 *  Copyright (c), 2017, Adrien Devresse
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 *
 */
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataSpace.hpp>
#include <highfive/H5File.hpp>
// Grox
#include "currency/currency.hpp"
#include "data/ohlc_datasets.hpp"
#include "data/ohlc_utils.hpp"
#include "indicators/indicator_definitions.hpp"
#include "io/hdf5_ohlc_manager.hpp"

std::string data_dir = "/home/biddisco/.local/share/grox";
std::string filename = "grox.hdf5";

TEST(moving_averages, moving_average)
{
  using namespace HighFive;
  namespace ba = boost::accumulators;

  hdf5_ohlc_manager data_manager;
  data_manager.init(data_dir, filename);

  const int N_samples = 128;
  QVector<ohlctv_sample> result;
  data_manager.read_file("bitstamp", "XRP-USD", result, N_samples);

  // get a reference to indicator in the global indicators list
  indicators::moving_average alg(15, 0, 2);

  // display
  int i = 0;
  std::stringstream tmp;
  for (auto ohlc : result)
  {
    // std::cout << i++ << " " << ohlc << std::endl;
    auto val = alg(ohlc);
    tmp << fmt::format("{:7.05f}, ", val);
  }
  std::string expected =
    "0.24750, 0.24656, 0.24625, 0.24610, 0.24650, 0.24687, 0.24713, 0.24737, 0.24755, 0.24769, "
    "0.24781, 0.24791, 0.24799, 0.24807, 0.24799, 0.24795, 0.24803, 0.24811, 0.24819, 0.24811, "
    "0.24798, 0.24786, 0.24771, 0.24754, 0.24738, 0.24721, 0.24704, 0.24688, 0.24673, 0.24673, "
    "0.24673, 0.24686, 0.24699, 0.24712, 0.24724, 0.24737, 0.24751, 0.24768, 0.24789, 0.24813, "
    "0.24835, 0.24850, 0.24860, 0.24876, 0.24892, 0.24908, 0.24918, 0.24930, 0.24941, 0.24955, "
    "0.24971, 0.24987, 0.25009, 0.25032, 0.25055, 0.25086, 0.25128, 0.25174, 0.25218, 0.25271, "
    "0.25318, 0.25374, 0.25443, 0.25518, 0.25588, 0.25656, 0.25741, 0.25818, 0.25899, 0.25986, "
    "0.26072, 0.26159, 0.26250, 0.26353, 0.26467, 0.26572, 0.26644, 0.26711, 0.26783, 0.26855, "
    "0.26927, 0.26995, 0.27070, 0.27152, 0.27233, 0.27302, 0.27355, 0.27426, 0.27470, 0.27481, "
    "0.27485, 0.27506, 0.27528, 0.27548, 0.27566, 0.27587, 0.27590, 0.27580, 0.27551, 0.27511, "
    "0.27490, 0.27484, 0.27460, 0.27459, 0.27482, 0.27532, 0.27585, 0.27627, 0.27660, 0.27693, "
    "0.27726, 0.27758, 0.27797, 0.27847, 0.27900, 0.27950, 0.28007, 0.28065, 0.28115, 0.28137, "
    "0.28145, 0.28142, 0.28134, 0.28115, 0.28076, 0.28032, 0.28012, 0.27989, ";
  // std::cout << tmp.str();
  EXPECT_EQ(expected, tmp.str());
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
