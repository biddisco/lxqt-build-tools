//------------------------------------------------------------------------------
//
// Example: WebSocket SSL client, asynchronous
//
//------------------------------------------------------------------------------

#include <openssl/ssl.h>

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
//
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
//
#include "network/websocket-ssl.hpp"

net::contexts io_contexts_;
std::vector<std::thread> ioc_threads_;
std::atomic<int> counter{0};

//------------------------------------------------------------------------------
void new_trade_data(std::string&& data)
{
  if (data.find("bts:subscription_succeeded") != data.npos)
  {
    counter++;
  }
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
  // Check command line arguments.
  if (argc != 4 && argc != 5)
  {
    std::cerr << "Usage  : bin/test-websocket <host> <port> <target> "
                 "[<HTTP version: 1.0 or 1.1(default)>]\n"
              << "Example:\n"
              << "bin/test-websocket ws.bitstamp.net 443 \"{\\\"event\\\": "
                 "\\\"bts:subscribe\\\",\\\"data\\\": {\\\"channel\\\": "
                 "\\\"live_trades_xrpusd\\\"}}\" \n";
    return EXIT_FAILURE;
  }

  start_io_threads(2);

  auto const host = argv[1];
  auto const port = argv[2];
  auto const channel = argv[3];
  std::cout << "Connecting : " << host << ":" << port << " " << channel << "\n";
  int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;

  using namespace std::placeholders;
  auto ws_orderbook = net::ws::create_session(
    io_contexts_.ioc, io_contexts_.ctx, argv[1], argv[2], channel, new_trade_data);

  int completed = 0;
  const int sec = 8;
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
    if (t.joinable())
      t.join();
  }

  std::cout << "Exiting" << std::endl;
  return (counter.load() > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
