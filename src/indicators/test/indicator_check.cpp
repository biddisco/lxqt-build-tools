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

namespace HighFive::details {
  //  template <>
  //  struct inspector<ohlctv_sample> : inspector<std::array<double, 6>>
  //  {
  //    static constexpr size_t ndim = 1;
  //    static constexpr size_t recursive_ndim = 0;
  //  };

  template <>
  struct inspector<ohlctv_sample>
  {
    using type = ohlctv_sample;
    using value_type = double;
    using base_type = typename inspector<value_type>::base_type;
    using hdf5_type = typename inspector<value_type>::hdf5_type;

    static constexpr size_t ndim = 1;
    static constexpr size_t recursive_ndim = 0;
    static constexpr bool is_trivially_copyable = true;

    static std::vector<size_t> getDimensions(const type& val)
    {
      std::vector<size_t> sizes{6};
      return sizes;
    }

    static size_t getSizeVal(const type& val)
    {
      return compute_total_size(getDimensions(val));
    }

    static size_t getSize(const std::vector<size_t>& dims)
    {
      return compute_total_size(dims);
    }

    static void prepare(type& /* val */, const std::vector<size_t>& dims)
    {
      //      if (dims[0] > N)
      //      {
      //        std::ostringstream os;
      //        os << "Size of std::array (" << N << ") is too small for dims (" << dims[0] << ").";
      //        throw DataSpaceException(os.str());
      //      }
    }

    static hdf5_type* data(type& val)
    {
      return inspector<value_type>::data(val.time);
    }

    static const hdf5_type* data(const type& val)
    {
      return inspector<value_type>::data(val.time);
    }

    template <class It>
    static void serialize(const type& val, It m)
    {
      //      size_t subsize = inspector<value_type>::getSizeVal(val[0]);
      //      for (auto& e : val)
      //      {
      //        inspector<value_type>::serialize(e, m);
      //        m += subsize;
      //      }
    }

    template <class It>
    static void unserialize(const It& vec_align, const std::vector<size_t>& dims, type& val)
    {
      //      if (dims[0] != N)
      //      {
      //        std::ostringstream os;
      //        os << "Impossible to pair DataSet with " << dims[0] << " elements into an array with " << N
      //           << " elements.";
      //        throw DataSpaceException(os.str());
      //      }
      //      std::vector<size_t> next_dims(dims.begin() + 1, dims.end());
      //      size_t next_size = compute_total_size(next_dims);
      //      for (size_t i = 0; i < dims[0]; ++i)
      //      {
      //        inspector<value_type>::unserialize(vec_align + i * next_size, next_dims, val[i]);
      //      }
    }
  };

}    // namespace HighFive::details

int main(void)
{
  using namespace HighFive;

  // open file
  File file(file_name, File::ReadOnly);

  // let's create a dataset of this size
  DataSet dataset = file.getDataSet(dataset_name);

  std::vector<ohlctv_sample> result;
  dataset.select({0}, {100}, {}, {}).read(result);

  int i = 0;
  // display
  for (auto ohlc : result)
  {
    std::cout << i++ << " " << ohlc << std::endl;
  }

  return 0;
}
