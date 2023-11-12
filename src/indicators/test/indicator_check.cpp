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

#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataSpace.hpp>
#include <highfive/H5File.hpp>

// Grox
#include "currency/currency.hpp"
#include "data/ohlc_datasets.hpp"
#include "data/ohlc_utils.hpp"
#include "io/hdf5_ohlc_manager.hpp"

const std::string file_name("/home/biddisco/.local/share/grox/grox.hdf5");
const std::string dataset_name("bitstamp/XRP-USD");

bool read_data(std::uint64_t size) {}

std::string data_dir = "/home/biddisco/.local/share/grox";
std::string filename = "grox.hdf5";

int main(void)
{
  using namespace HighFive;

  hdf5_ohlc_manager data_manager;
  data_manager.init(data_dir, filename);

  const int N_samples = 256;
  QVector<ohlctv_sample> result;
  data_manager.read_file("bitstamp", "XRP-USD", result, N_samples);

  int i = 0;
  // display
  for (auto ohlc : result)
  {
    std::cout << i++ << " " << ohlc << std::endl;
  }

  return 0;
}
