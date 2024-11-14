#pragma once
// stl
#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>
// grox
#include <pika/concurrency/spinlock.hpp>
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"

// ----------------------------------------------------------------------------
template <int Level>
inline constexpr grox::debug::print_threshold<Level, 5> pubsub_dbg("PubSub  ");

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

    void publish(Message... message) const
    {
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      if (subscriptions.size() > 0)
      {
        using namespace grox::debug;
        for (auto& subscriber : subscriptions)
        {
          pubsub_dbg<6>.debug(str<>("publish"), subscriber.first, print_type<Signature>());
          subscriber.second(message...);
        }
      }
    }

    void subscribe(std::string const& id, Signature callback)
    {
      using namespace grox::debug;
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      if (subscriptions.contains(id))
      {
        pubsub_dbg<0>.error(str<>("duplicate subscribe"), id, print_type<Signature>());
      }
      subscriptions.insert(std::make_pair(id, callback));
    }

    void unsubscribe(std::string const& id)
    {
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      if (subscriptions.contains(id))
        subscriptions.erase(id);
      else
        throw std::runtime_error("Incorrect Id given to unsubscribe");
    }

    void clear()
    {
      std::lock_guard<mutex_type> lk(add_remove_mtx_);
      subscriptions.clear();
    }
  };

}    // namespace grox
