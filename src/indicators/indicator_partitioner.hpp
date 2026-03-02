#pragma once

#include <cstddef>
#include <cstdint>
//
#include "debug/logging.hpp"
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
      GROX_LOG_DEBUG(indicator_log, "{:>20} {:03d} {} {} {:08d}", "Partition create",
          num_partitions_, static_cast<void const*>(&*input.begin()),
          static_cast<void const*>(&*input.end()), input.size());
    }

    extent get_partition(int piece) const
    {
      Iterator begin = std::next(origin_.begin, piece * chunksize_);
      Iterator end = std::min(std::next(origin_.begin, (piece + 1) * chunksize_), origin_.end);
      GROX_LOG_DEBUG(indicator_log, "{:>20} {:03d} of {:03d} {},{} {} {}", "Partition get", piece,
          num_partitions_, static_cast<void const*>(&*begin), static_cast<void const*>(&*end),
          begin - origin_.begin, end - origin_.begin);
      return {begin, end};
    }
    //
    std::uint64_t chunksize_;
    std::uint64_t num_partitions_;
    extent origin_;
  };
}    // namespace indicators
