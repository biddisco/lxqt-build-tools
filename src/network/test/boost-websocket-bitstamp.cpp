//------------------------------------------------------------------------------
//
// Example: WebSocket SSL client, asynchronous
//
//------------------------------------------------------------------------------

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
//
#include <openssl/ssl.h>
//
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
//
#include "network/test/websocket-ssl.hpp"
//
#include "test-options.hpp"

//------------------------------------------------------------------------------
net::contexts io_contexts_;
std::vector<std::thread> ioc_threads_;
std::atomic<int> counter{0};

//------------------------------------------------------------------------------
void new_trade_data(std::string&& data)
{
  if (data.find("bts:subscription_succeeded") != data.npos) { counter++; }
  std::cout << "\n\nReceived\n\n" << data << std::endl;
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

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
  auto vm = set_program_options(argc, argv,
      {{"url", "www.bitstamp.net"}, {"port", "443"},
          {"target", "/api/v2/ohlc/xrpusd/?step=60&limit=10"}});
  std::string url = vm["url"].as<std::string>();
  std::string port = vm["port"].as<std::string>();
  std::string target = vm["target"].as<std::string>();

  start_io_threads(2);
  std::cout << "Connecting : " << url << ":" << port << " " << target << "\n";

  using namespace std::placeholders;
  auto ws_orderbook = net::ws::create_session(
      io_contexts_.ioc, io_contexts_.ctx, url, port, target, new_trade_data);

  int completed = 0;
  int const sec = 8;
  // wait 5 seconds and collect some data
  for (int i = 0; i < sec && (counter.load() < 2); i++)
  {
    std::cout << "Closing in " << sec - i << " seconds " << std::endl;
    std::chrono::seconds dura(1);
    std::this_thread::sleep_for(dura);
  }

  ws_orderbook->shutdown();
  ws_orderbook.reset();
  std::cout << "Completed " << completed << std::endl;

  //
  io_contexts_.work_guard_->reset();
  io_contexts_.ioc.stop();
  for (auto& t : ioc_threads_)
  {
    if (t.joinable()) t.join();
  }

  std::cout << "Exiting" << std::endl;
  return (counter.load() > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
