#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
//
#include <QStringLiteral>
#include <QThread>
#include <QtCore/QCoreApplication>
//
#include "currency/currency.hpp"
#include "currency/currency_pair.hpp"
#include "network/qwebsocket_client.hpp"
#include "network/qwebsocket_session.hpp"
//
#include "test-options.hpp"

//std::string const server_addr = "https://s1.ripple.com:51234";
std::string url = "https://s1.ripple.com";
std::string port = "51234";
int test_timeout{5};
std::atomic<int> counter{0};

// ----------------------------------------------------------------------------
static void new_orderbook_data(QString data)
{
  std::string stdstring = data.toStdString();
  nlohmann::json jdata = nlohmann::json::parse(stdstring);
  if (jdata.contains("transaction"))
  {
    counter++;
    std::cout << jdata["transaction"].dump(4) << std::endl;
  }
}

// ----------------------------------------------------------------------------
void websocket_subscribe_offers()
{
  //startswith
  nlohmann::json command;
  command["command"] = "subscribe";
  // buying xrp with bitstamp USD
  nlohmann::json buy_xrp;
  buy_xrp["taker_gets"]["currency"] = "XRP";
  buy_xrp["taker_pays"]["currency"] = "USD";
  buy_xrp["taker_pays"]["issuer"] = currency_issuers::bitstamp_trust;
  buy_xrp["snapshot"] = true;
  // selling xrp with bitstamp USD
  nlohmann::json sell_xrp;
  sell_xrp["taker_pays"]["currency"] = "XRP";
  sell_xrp["taker_gets"]["currency"] = "USD";
  sell_xrp["taker_gets"]["issuer"] = currency_issuers::bitstamp_trust;
  sell_xrp["snapshot"] = true;
  // subscribe to 2 books
  command["books"] = nlohmann::json::array({buy_xrp, sell_xrp});
  command["ledger_index"] = "current";
  std::string subscription = command.dump();

  //
  std::shared_ptr<net::ws::qwebsocket_session> websocket = net::ws::qwebsocket_session::create(
      "xrpl::orderbook", url, std::atoi(port.c_str()), subscription, new_orderbook_data);

  // wait N seconds and collect some data, hope for at least 2 offers before timeout
  for (int i = 0; i < test_timeout && (counter.load() < 2); i++)
  {
    std::cout << "Closing in " << test_timeout - i << " seconds " << std::endl;
    std::chrono::seconds dura(1);
    std::this_thread::sleep_for(dura);
  }

  websocket.reset();
}

// ------------------------------------------------------------------
int main(int argc, char* argv[])
{
  auto vm = set_program_options(
      argc, argv, {{"url", url}, {"port", port}, {"timeout", std::to_string(test_timeout)}});
  url = vm["url"].as<std::string>();
  port = vm["port"].as<std::string>();
  test_timeout = vm["timeout"].as<int>();
  // std::string target = vm["target"].as<std::string>();
  QCoreApplication a(argc, argv);

  websocket_subscribe_offers();
  return counter.load() > 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
