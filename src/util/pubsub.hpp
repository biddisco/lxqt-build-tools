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
    std::vector<Signature> subscriptions;

    void publish(Message... message) const
    {
      if (subscriptions.size() > 0)
      {
        using namespace grox::debug;
        pubsub_dbg<6>.debug(str<>("publish"), print_type<Signature>());
        for (auto& subscriber : subscriptions) { subscriber(message...); }
      }
    }

    void subscribe(Signature callback) { subscriptions.push_back(callback); }

    void clear() { subscriptions.clear(); }
  };

}    // namespace grox
