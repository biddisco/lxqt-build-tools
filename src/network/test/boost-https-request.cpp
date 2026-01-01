
//------------------------------------------------------------------------------
//
// Example: HTTP SSL client, asynchronous
//
//------------------------------------------------------------------------------

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <thread>
//
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <openssl/ssl.h>
//
#include "network/test/https-async.hpp"
#include "test-options.hpp"

std::atomic<int> counter{0};
//------------------------------------------------------------------------------
//
// Creates a session using https and fetches data from bitstamp
//
//------------------------------------------------------------------------------
void new_data(std::string&& data)
{
  std::cout << "\n\nReceived\n\n" << data << std::endl;
  //
  std::regex response_regex(
      ".*\"data\".*\"pair\".*\"(.*)\".*\"ohlc\".*", std::regex_constants::extended);
  std::smatch mtch;
  if (std::regex_match(data, mtch, response_regex))
  {
    std::string token = mtch[1];
    std::cout << "json regex check ok " << token << std::endl;
    counter++;
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

  std::cout << "Connecting : " << url << ":" << port << " " << target << "\n";
  int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;

  // The io_context is required for all I/O
  net::contexts contexts;

  std::shared_ptr<net::https::session> session =
      net::https::create_session(contexts.ioc, contexts.ctx, url, port, new_data);

  // Run the I/O service on a thread.
  std::thread io_thread([&]() {
    // The call will return when the socket is closed.
    contexts.ioc.run();
  });

  // wait until connection is setup
  while (!session->ready_) { std::this_thread::yield(); }

  // invoke a post on the context thread
  contexts.ioc.post([&]() {
    std::cout << "IO context::post ok" << std::endl;
    session->write(target, version);
  });

  int const sec = 5;
  // wait 5 seconds and collect some data
  for (int i = 0; i < sec && (counter.load() == 0); i++)
  {
    std::cout << "Closing in " << sec - i << " seconds " << std::endl;
    std::chrono::seconds dura(1);
    std::this_thread::sleep_for(dura);
  }

  session->shutdown_blocking();
  session.reset();
  std::cout << "Completed " << std::endl;
  //
  contexts.work_guard_->reset();
  contexts.ioc.stop();
  io_thread.join();

  std::cout << "Exiting" << std::endl;
  return (counter.load() > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
