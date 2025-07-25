#pragma once

#include <stdexec/execution.hpp>
//
#include <pika/assert.hpp>
#include <pika/config.hpp>
//
#include <pika/modules/execution.hpp>
#include <pika/modules/executors.hpp>
#include <pika/modules/resource_partitioner.hpp>
#include <pika/modules/runtime.hpp>

// -----------------------------------------------------------------
// return a scheduler on the default pool with added priority if requested
namespace grox::senders {
  namespace pexec = pika::execution;
  namespace ex = pika::execution::experimental;

  inline auto default_pool_scheduler(pexec::thread_priority p = pexec::thread_priority::normal)
  {
    return ex::with_priority(
        ex::thread_pool_scheduler{&pika::resource::get_thread_pool("default")}, p);
  }
}    // namespace grox::senders
