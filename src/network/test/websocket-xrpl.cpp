#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
//
#include "currency/currency.hpp"
#include "network/websocket-ssl.hpp"

net::contexts io_contexts_;
std::vector<std::thread> ioc_threads_;
std::atomic<int> counter{0};

// ----------------------------------------------------------------------------
static void new_orderbook_data(void* nw, currency_pair const cp, std::string_view data)
{
  if (data.find("{\"result\":{\"offers\"") != data.npos)
  {
    counter++;
  }
  std::string_view slice = data.substr(0, std::min(std::size_t(64), data.length()));
  std::cout << slice << std::endl;
}

// ----------------------------------------------------------------------------
void start_io_threads(int nthreads)
{
  static bool initialized = false;
  if (!initialized)
  {
    ioc_threads_.reserve(nthreads);
    // Run the I/O service on some threads.
    for (int i = 0; i < nthreads; ++i)
    {
      ioc_threads_.emplace_back([&]() {
        // The call will return when the socket is closed.
        io_contexts_.ioc.run();
      });
    }
    initialized = true;
  }
}

// ----------------------------------------------------------------------------
void websocket_subscribe_offers()
{
  //startswith
  nlohmann::json command;
  command["command"] = "subscribe";
  // buying xrp
  nlohmann::json buy_xrp;
  buy_xrp["taker_gets"]["currency"] = "XRP";
  buy_xrp["taker_pays"]["currency"] = "USD";
  buy_xrp["taker_pays"]["issuer"] = currency::bitstamp_trust;
  buy_xrp["snapshot"] = true;
  // selling xrp
  nlohmann::json sell_xrp;
  sell_xrp["taker_pays"]["currency"] = "XRP";
  sell_xrp["taker_gets"]["currency"] = "USD";
  sell_xrp["taker_gets"]["issuer"] = currency::bitstamp_trust;
  sell_xrp["snapshot"] = true;
  // subscribe to 2 books
  command["books"] = nlohmann::json::array({buy_xrp, sell_xrp});
  std::string subscription = command.dump();

  using namespace std::placeholders;
  auto ws_orderbook = net::ws::create_session(io_contexts_.ioc, io_contexts_.ctx, "s1.ripple.com",
    "443", subscription, std::bind(new_orderbook_data, nullptr, currency_pair{}, _1));

  int completed = 0;
  const int sec = 8;
  // wait 5 seconds and collect some data
  for (int i = 0; i < sec && (counter.load() == 0); i++)
  {
    std::cout << "Closing in " << sec - i << " seconds " << std::endl;
    std::chrono::seconds dura(1);
    std::this_thread::sleep_for(dura);
  }

  ws_orderbook->shutdown();
  ws_orderbook.reset();
  std::cout << "Completed " << completed << std::endl;
}

int main(int argc, char* argv[])
{
  start_io_threads(2);
  websocket_subscribe_offers();
  //
  io_contexts_.work_guard_->reset();
  io_contexts_.ioc.stop();
  for (auto& t : ioc_threads_)
  {
    if (t.joinable())
      t.join();
  }

  std::cout << "Exiting" << std::endl;
  return (counter.load() > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
