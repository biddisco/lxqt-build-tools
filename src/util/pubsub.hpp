#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
//
#include <stdexec/execution.hpp>
//
#include <pika/concurrency/spinlock.hpp>
#include <pika/execution/algorithms/just.hpp>
#include <pika/execution/algorithms/transfer_just.hpp>
#include <pika/execution_base/any_sender.hpp>
//
#include "debug/demangle_helper.hpp"
#include "debug/logging.hpp"
#include "senders/pika_stdexec.hpp"

// ----------------------------------------------------------------------------
inline auto pubsub_log = grox::log::create("Pub__Sub");

// ----------------------------------------------------------------------------
namespace grox {

  template <typename... Message>
  struct PublishSubscribe
  {
    /// Spinlock is used as it can be called by OS threads or pika tasks
    using mutex_type = pika::detail::spinlock;
    /// each pubsub instance is based on the callback type
    using Signature = std::function<void(Message...)>;
    std::unordered_map<std::string, Signature> subscriptions;
    mutable mutex_type add_remove_mtx_;

    // ----------------------------------------------------------------------------
    ~PublishSubscribe() { clear(); }

    // ----------------------------------------------------------------------------
    void publish(Message... message) const
    {
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      if (subscriptions.size() > 0)
      {
        using grox::debug::print_type;
        for (auto& subscriber : subscriptions)
        {
          GROX_LOG_TRACE(
              pubsub_log, "{:>20} {} {}", "publish", subscriber.first, print_type<Signature>());

          stdexec::sender auto snd =
              stdexec::starts_on(grox::senders::default_pool_scheduler(), stdexec::just()) |
              stdexec::then([=]() { subscriber.second(message...); });
          pika::execution::experimental::start_detached(std::move(snd));
        }
      }
    }

    // ----------------------------------------------------------------------------
    void subscribe(std::string const& id, Signature callback)
    {
      using grox::debug::print_type;
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      if (subscriptions.contains(id))
      {
        GROX_LOG_ERROR(
            pubsub_log, "{:>20} {} {}", "duplicate subscribe", id, print_type<Signature>());
      }
      subscriptions.insert(std::make_pair(id, callback));
    }

    // ----------------------------------------------------------------------------
    void unsubscribe(std::string const& id)
    {
      using grox::debug::print_type;
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      if (subscriptions.contains(id)) { subscriptions.erase(id); }
      else
      {
        if (subscriptions.empty())
        {
          // During shutdown, it's normal for pub/sub to be cleared before all unsubscribe calls complete.
          // This is not an error condition, so log at DEBUG level instead of ERROR.
          GROX_LOG_DEBUG(pubsub_log, "{:>20} {} {} {}", "unsubscribe", id, "already_cleared",
              print_type<Signature>());
        }
        else
        {
          for ([[maybe_unused]] auto const& subscriber : subscriptions)
          {
            GROX_LOG_ERROR(pubsub_log, "{:>20} {} {} {}", "unsubscribe", id, subscriber.first,
                print_type<Signature>());
          }
          throw std::runtime_error("Incorrect Id given to unsubscribe");
        }
      }
    }

    // ----------------------------------------------------------------------------
    void clear()
    {
      using grox::debug::print_type;
      for ([[maybe_unused]] auto const& subscriber : subscriptions)
      {
        GROX_LOG_TRACE(pubsub_log, "{:>20} {} {} {}", "unsubscribe", "clear", subscriber.first,
            print_type<Signature>());
      }
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      subscriptions.clear();
    }
  };

}    // namespace grox
