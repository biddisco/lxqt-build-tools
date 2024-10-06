#pragma once
// stl
#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace grox {

  template <typename... Message>
  struct PublishSubscribe
  {
    using Signature = std::function<void(Message...)>;
    //
    std::vector<Signature> subscriptions;

    void publish(Message&... message)
    {
      for (auto& subscriber : subscriptions)
      {
        subscriber(message...);
      }
    }

    void subscribe(Signature callback)
    {
      subscriptions.push_back(callback);
    }

    void clear()
    {
      subscriptions.clear();
    }
  };

}    // namespace grox
