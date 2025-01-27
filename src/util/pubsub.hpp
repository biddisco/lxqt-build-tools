#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
//
#include <pika/concurrency/spinlock.hpp>
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug::detail;
template <int Level>
inline constexpr print_threshold<Level, 2> pubsub_dbg("PubSub  ");

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
        using namespace grox::debug;
        for (auto& subscriber : subscriptions)
        {
          pubsub_dbg<6>.debug(ffmt<s20>("publish"), subscriber.first, print_type<Signature>());
          subscriber.second(message...);
        }
      }
    }

    // ----------------------------------------------------------------------------
    void subscribe(std::string const& id, Signature callback)
    {
      using namespace grox::debug;
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      if (subscriptions.contains(id))
      {
        pubsub_dbg<0>.error(ffmt<s20>("duplicate subscribe"), id, print_type<Signature>());
      }
      subscriptions.insert(std::make_pair(id, callback));
    }

    // ----------------------------------------------------------------------------
    void unsubscribe(std::string const& id)
    {
      using namespace grox::debug;
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      if (subscriptions.contains(id)) { subscriptions.erase(id); }
      else
      {
        if (subscriptions.empty())
        {
          pubsub_dbg<0>.error(
              ffmt<s20>("unsubscribe"), id, "empty/cleared", print_type<Signature>());
        }
        else
        {
          for (auto const& [k, v] : subscriptions)
          {
            pubsub_dbg<0>.error(ffmt<s20>("unsubscribe"), id, k, print_type<Signature>());
          }
          throw std::runtime_error("Incorrect Id given to unsubscribe");
        }
      }
    }

    // ----------------------------------------------------------------------------
    void clear()
    {
      using namespace grox::debug;
      for (auto const& [k, v] : subscriptions)
      {
        pubsub_dbg<3>.debug(ffmt<s20>("unsubscribe"), "clear", k, print_type<Signature>());
      }
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      subscriptions.clear();
    }
  };

}    // namespace grox
