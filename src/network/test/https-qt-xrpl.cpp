#include <iostream>
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

static std::atomic<int> pass_count{0};

void handler(net::http::client_ptr client, std::string_view reply)
{
  // print the full response
  std::cerr << reply << "\n\n";
  if (reply.find("{\"result\":{\"ledger_hash\":") != std::string::npos)
  {
    std::cout << "json quick check ok\n\n";
    pass_count++;
    if (pass_count == 1)
    {
      QCoreApplication::quit();
    }
  }
  else
  {
    pass_count--;
  }
  client.reset();
}

int main(int argc, char* argv[])
{
  QCoreApplication a(argc, argv);

  nlohmann::json content;
  content["method"] = "book_offers";
  //
  nlohmann::json paramlist;
  paramlist["taker_gets"]["currency"] = "XRP";
  paramlist["taker_pays"]["currency"] = "USD";
  paramlist["taker_pays"]["issuer"] = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
  paramlist["limit"] = 10;
  //
  content["params"] = nlohmann::json::array({paramlist});

  QNetworkAccessManager networkmanager;
  net::http::client_ptr client = net::http::qhttp_request_client::create(
    networkmanager, "https://s1.ripple.com:51234", content.dump(), &handler);
  client->post_json_request();

  a.exec();
  std::cout << "received " << pass_count.load() << std::endl;
  return pass_count.load() == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
