#pragma once

#include <cstddef>
#include <cstdint>
//
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
namespace indicators {

  template <typename Container>
  struct block_partitioner
  {
    using Iterator = Container::const_iterator;

    struct extent
    {
      Iterator begin;
      Iterator end;
    };

    block_partitioner(Container const& input, std::uint64_t start, std::uint64_t chunksize)
    {
      chunksize_ = chunksize;
      num_partitions_ = std::ceil(static_cast<double>(input.size() - start) / chunksize);
      // set extent from begin of partition 0, the end of data
      origin_.begin = std::next(input.begin(), start);
      origin_.end = input.end();
      indicator_dbg<5>.debug(ffmt<s20>("Partition create"), ffmt<dec3>(num_partitions_),
          static_cast<void const*>(&*input.begin()), static_cast<void const*>(&*input.end()),
          ffmt<dec8>(input.size()));
    }

    extent get_partition(int piece) const
    {
      Iterator begin = std::next(origin_.begin, piece * chunksize_);
      Iterator end = std::min(std::next(origin_.begin, (piece + 1) * chunksize_), origin_.end);
      indicator_dbg<2>.debug(ffmt<s20>("Partition get"), ffmt<dec3>(piece), "of",
          ffmt<dec3>(num_partitions_),
          fmt::format("{},{}", static_cast<void const*>(&*begin), static_cast<void const*>(&*end)),
          begin - origin_.begin, end - origin_.begin);
      return {begin, end};
    }
    //
    std::uint64_t chunksize_;
    std::uint64_t num_partitions_;
    extent origin_;
  };
}    // namespace indicators
