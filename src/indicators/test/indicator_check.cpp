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

using namespace HighFive;
namespace ba = boost::accumulators;

TEST(moving_averages, moving_average)
{
  hdf5_ohlc_manager data_manager;
  data_manager.init(data_dir, filename);

  const int N_samples = 128;
  QVector<ohlctv_sample> result;
  data_manager.read_file("bitstamp", "XRP-USD", result, N_samples);

  // get a reference to indicator in the global indicators list
  indicators::moving_average alg(15, ohlc_modes::mid_open_close);

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

TEST(moving_averages, exponential_moving_average)
{
  hdf5_ohlc_manager data_manager;
  data_manager.init(data_dir, filename);

  const int N_samples = 128;
  QVector<ohlctv_sample> result;
  data_manager.read_file("bitstamp", "XRP-USD", result, N_samples);

  // get a reference to indicator in the global indicators list
  indicators::moving_average_exponential alg(15, ohlc_modes::mid_open_close);
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
    "0.24750, 0.24727, 0.24706, 0.24688, 0.24704, 0.24725, 0.24743, 0.24763, 0.24780, 0.24795, "
    "0.24808, 0.24819, 0.24830, 0.24838, 0.24820, 0.24803, 0.24788, 0.24775, 0.24764, 0.24754, "
    "0.24746, 0.24738, 0.24730, 0.24720, 0.24712, 0.24704, 0.24698, 0.24692, 0.24690, 0.24690, "
    "0.24690, 0.24713, 0.24734, 0.24752, 0.24768, 0.24780, 0.24795, 0.24812, 0.24831, 0.24853, "
    "0.24870, 0.24871, 0.24863, 0.24869, 0.24877, 0.24883, 0.24900, 0.24920, 0.24937, 0.24955, "
    "0.24973, 0.24995, 0.25029, 0.25062, 0.25098, 0.25144, 0.25188, 0.25228, 0.25272, 0.25327, "
    "0.25366, 0.25427, 0.25510, 0.25593, 0.25661, 0.25718, 0.25807, 0.25885, 0.25962, 0.26049, "
    "0.26137, 0.26219, 0.26302, 0.26404, 0.26532, 0.26616, 0.26656, 0.26712, 0.26779, 0.26835, "
    "0.26879, 0.26951, 0.27025, 0.27114, 0.27211, 0.27282, 0.27321, 0.27399, 0.27445, 0.27465, "
    "0.27439, 0.27416, 0.27418, 0.27434, 0.27442, 0.27450, 0.27456, 0.27448, 0.27429, 0.27413, "
    "0.27419, 0.27429, 0.27449, 0.27487, 0.27544, 0.27602, 0.27658, 0.27708, 0.27751, 0.27782, "
    "0.27808, 0.27828, 0.27847, 0.27873, 0.27901, 0.27939, 0.27992, 0.28050, 0.28107, 0.28128, "
    "0.28127, 0.28112, 0.28090, 0.28050, 0.27971, 0.27891, 0.27863, 0.27834, ";
  // std::cout << tmp.str();
  EXPECT_EQ(expected, tmp.str());
}

TEST(moving_averages, volume_weighted_moving_average)
{
  hdf5_ohlc_manager data_manager;
  data_manager.init(data_dir, filename);

  const int N_samples = 128;
  QVector<ohlctv_sample> result;
  data_manager.read_file("bitstamp", "XRP-USD", result, N_samples);

  // get a reference to indicator in the global indicators list
  indicators::moving_average_volume_weighted alg(15, ohlc_modes::mid_open_close);
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
    "0.24750, 0.24563, 0.24563, 0.24563, 0.24810, 0.24810, 0.24810, 0.24813, 0.24813, 0.24813, "
    "0.24813, 0.24813, 0.24813, 0.24813, 0.24810, 0.24810, 0.24810, 0.24810, 0.24782, 0.24701, "
    "0.24697, 0.24696, 0.24685, 0.24685, 0.24685, 0.24685, 0.24685, 0.24685, 0.24683, 0.24683, "
    "0.24683, 0.24696, 0.24696, 0.24699, 0.24701, 0.24723, 0.24845, 0.24853, 0.24902, 0.24911, "
    "0.24918, 0.24914, 0.24892, 0.24908, 0.24908, 0.24908, 0.24921, 0.24921, 0.24935, 0.24942, "
    "0.24969, 0.25037, 0.25078, 0.25134, 0.25150, 0.25199, 0.25222, 0.25251, 0.25254, 0.25291, "
    "0.25337, 0.25436, 0.25524, 0.25574, 0.25621, 0.25667, 0.25762, 0.25809, 0.25969, 0.25993, "
    "0.26094, 0.26174, 0.26255, 0.26338, 0.26503, 0.26605, 0.26709, 0.26796, 0.26864, 0.26932, "
    "0.27006, 0.27045, 0.27082, 0.27203, 0.27419, 0.27536, 0.27567, 0.27618, 0.27653, 0.27672, "
    "0.27608, 0.27609, 0.27610, 0.27621, 0.27622, 0.27656, 0.27660, 0.27662, 0.27649, 0.27537, "
    "0.27396, 0.27391, 0.27376, 0.27380, 0.27404, 0.27552, 0.27562, 0.27627, 0.27823, 0.27857, "
    "0.27862, 0.27862, 0.27872, 0.27900, 0.27976, 0.28003, 0.28052, 0.28111, 0.28140, 0.28154, "
    "0.28152, 0.28152, 0.28153, 0.28151, 0.28148, 0.28118, 0.28117, 0.28109, ";
  // std::cout << tmp.str();
  EXPECT_EQ(expected, tmp.str());
}

TEST(moving_averages, moving_average_exponential_volume_weighted)
{
  hdf5_ohlc_manager data_manager;
  data_manager.init(data_dir, filename);

  const int N_samples = 128;
  QVector<ohlctv_sample> result;
  data_manager.read_file("bitstamp", "XRP-USD", result, N_samples);

  // get a reference to indicator in the global indicators list
  indicators::moving_average_exponential_volume_weighted alg(15, ohlc_modes::mid_open_close);
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
    "0.03094, 0.05777, 0.08126, 0.10180, 0.12009, 0.13609, 0.15009, 0.16235, 0.17307, 0.18245, "
    "0.19066, 0.19785, 0.20413, 0.20963, 0.21444, 0.21865, 0.22233, 0.22555, 0.22834, 0.23067, "
    "0.23271, 0.23449, 0.23603, 0.23739, 0.23857, 0.23960, 0.24051, 0.24130, 0.24199, 0.24260, "
    "0.24313, 0.24361, 0.24402, 0.24440, 0.24472, 0.24504, 0.24546, 0.24585, 0.24624, 0.24660, "
    "0.24692, 0.24720, 0.24742, 0.24762, 0.24781, 0.24797, 0.24812, 0.24826, 0.24839, 0.24852, "
    "0.24867, 0.24888, 0.24912, 0.24940, 0.24966, 0.24995, 0.25023, 0.25052, 0.25077, 0.25104, "
    "0.25133, 0.25171, 0.25215, 0.25260, 0.25305, 0.25350, 0.25402, 0.25453, 0.25517, 0.25577, "
    "0.25641, 0.25708, 0.25776, 0.25846, 0.25929, 0.26013, 0.26100, 0.26187, 0.26272, 0.26354, "
    "0.26436, 0.26512, 0.26583, 0.26661, 0.26755, 0.26853, 0.26942, 0.27027, 0.27105, 0.27176, "
    "0.27230, 0.27277, 0.27319, 0.27357, 0.27390, 0.27423, 0.27453, 0.27479, 0.27500, 0.27505, "
    "0.27491, 0.27479, 0.27466, 0.27455, 0.27449, 0.27462, 0.27474, 0.27493, 0.27535, 0.27575, "
    "0.27611, 0.27642, 0.27671, 0.27699, 0.27734, 0.27768, 0.27803, 0.27842, 0.27879, 0.27913, "
    "0.27943, 0.27969, 0.27992, 0.28012, 0.28029, 0.28040, 0.28050, 0.28057, ";
  // std::cout << tmp.str();
  EXPECT_EQ(expected, tmp.str());
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
