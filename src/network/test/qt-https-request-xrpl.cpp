#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>
//
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
//
#include <nlohmann/json.hpp>
//
#include "network/qhttp-request-client.hpp"
#include "test-options.hpp"

//std::string const server_addr = "https://s1.ripple.com:51234";
std::string url = "https://s1.ripple.com";
std::string port = "51234";
static std::atomic<int> pass_count{0};

void handler(QByteArray byteArray)
{
  std::string_view reply(byteArray.constData(), byteArray.length());
  // print the full response
  std::cerr << reply << "\n\n";
  if (reply.find("{\"result\":{\"ledger_current_index\":") != std::string::npos)
  {
    std::cout << "json quick check ok\n\n";
    pass_count++;
    if (pass_count == 1) { QCoreApplication::quit(); }
  }
  else { pass_count--; }
}

int main(int argc, char* argv[])
{
  auto vm = set_program_options(argc, argv, {{"url", url}, {"port", port}});
  url = vm["url"].as<std::string>();
  port = vm["port"].as<std::string>();
  //
  QCoreApplication a(argc, argv);
  QNetworkAccessManager networkmanager;

  nlohmann::json content;
  content["method"] = "book_offers";
  //
  nlohmann::json paramlist;
  paramlist["taker_gets"]["currency"] = "XRP";
  paramlist["taker_pays"]["currency"] = "USD";
  paramlist["taker_pays"]["issuer"] = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
  paramlist["ledger_index"] = "current";
  paramlist["limit"] = 10;
  //
  content["params"] = nlohmann::json::array({paramlist});

  net::http::client_ptr client =
      net::http::qhttp_request_client::create(networkmanager, url + ":" + port, content.dump());
  client->post_request(handler);

  a.exec();
  std::cout << "received " << pass_count.load() << std::endl;
  return pass_count.load() == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
