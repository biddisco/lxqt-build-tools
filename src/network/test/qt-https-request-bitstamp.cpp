#include <iostream>
//
#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
//
#include <fmt/format.h>
//
#include "network/qhttp-request-client.hpp"

static std::atomic<int> pass_count{0};
std::vector<std::string> test_list{"xrpusd", "btcusd", "xrpeur", "btceur", "xrpbtc"};

void handler(QByteArray byteArray)
{
  std::string_view reply(byteArray.constData(), byteArray.length());
  // print the full response
  std::cerr << "Response:\n" << reply << "\n\n";
  if (reply.find("{\"data\": {\"ohlc\": [{") != std::string::npos)
  {
    std::cout << "json quick check ok\n\n";
    pass_count++;
    if (pass_count == test_list.size())
    {
      QCoreApplication::quit();
    }
  }
  else
  {
    pass_count--;
  }
}

int main(int argc, char* argv[])
{
  QCoreApplication a(argc, argv);
  QNetworkAccessManager networkmanager;

  for (auto& cp : test_list)
  {
    std::string url = fmt::format("https://{}:{}/api/v2/ohlc/{}/?step=60&start=1704048480&limit=10",
      "www.bitstamp.net", 443, cp);
    auto* client = net::http::qhttp_request_client::create(networkmanager, url);
    client->get_request(&handler);
  }
  a.exec();
  //
  std::cout << "received " << pass_count.load() << std::endl;
  return pass_count.load() == 5 ? EXIT_SUCCESS : EXIT_FAILURE;
}
