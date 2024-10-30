#pragma once
// stl
#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>
// grox
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
    using Signature = std::function<void(Message...)>;
    std::unordered_map<std::string, Signature> subscriptions;

    void publish(Message... message) const
    {
      if (subscriptions.size() > 0)
      {
        using namespace grox::debug;
        pubsub_dbg<6>.debug(str<>("publish"), print_type<Signature>());
        for (auto& subscriber : subscriptions) { subscriber.second(message...); }
      }
    }

    void subscribe(std::string const& id, Signature callback)
    {
      subscriptions.insert(std::make_pair(id, callback));
    }

    void unsubscribe(std::string const& id) { subscriptions.erase(id); }

    void clear() { subscriptions.clear(); }
  };

}    // namespace grox
