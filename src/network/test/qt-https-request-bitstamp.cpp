#include <iostream>
#include <regex>
//
#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
//
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
//
#include "network/qhttp-request-client.hpp"

static std::atomic<int> pass_count{0};
static int const numcandles = 10;
// std::vector<std::string> test_list{"xrpusd", "btcusd", "xrpeur", "btceur", "xrpbtc"};
std::vector<std::string> test_list{"xrpusd", "btcusd"};

void handler(QByteArray byteArray)
{
  // not using string_view because regex wants a string
  std::string reply(byteArray.constData(), byteArray.length());
  // print the full response
  std::cerr << "Response:\n" << reply << "\n\n";

  std::regex response_regex(
      ".*\"data\".*\"pair\".*\"(.*)\".*\"ohlc\".*", std::regex_constants::extended);
  std::smatch mtch;
  if (std::regex_match(reply, mtch, response_regex))
  {
    std::string token = mtch[1];
    std::cout << "json regex check ok " << token << std::endl;
    //
    nlohmann::json jdata = nlohmann::json::parse(reply);
    int N = jdata["data"]["ohlc"].size();
    std::cout << "json size " << token << " " << N << std::endl << std::endl;
    EXPECT_EQ(N, numcandles);

    pass_count++;
    if (pass_count == test_list.size()) { QCoreApplication::quit(); }
  }
  else { pass_count--; }
}

//----------------------------------------------------------------------------
TEST(bitstamp, OHLC)
{
  int argc = 0;
  char** argv = nullptr;
  QCoreApplication a(argc, argv);
  QNetworkAccessManager networkmanager;
  //
  for (auto& cp : test_list)
  {
    std::string url = fmt::format("https://{}:{}/api/v2/ohlc/{}/?step=60&start=1704048480&limit={}",
        "www.bitstamp.net", 443, cp, numcandles);
    auto* client = net::http::qhttp_request_client::create(networkmanager, url);
    client->get_request(&handler);
  }
  a.exec();
  //
  std::cout << "received " << pass_count.load() << std::endl;
  EXPECT_EQ(pass_count.load(), test_list.size());
}

//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
