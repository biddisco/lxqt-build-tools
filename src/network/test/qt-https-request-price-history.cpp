#include <iostream>
#include <thread>
//
#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
//
#include <fmt/format.h>
#include "nlohmann/json.hpp"
//
#include "debug/print.hpp"
#include "network/qhttp-request-client.hpp"
#include "util/datetime_utils.hpp"

// ----------------------------------------------------------------------------
static std::atomic<int> counter{0};

// ----------------------------------------------------------------------------
using namespace grox::debug::detail;
//
template <int Level>
inline constexpr print_threshold<Level, 6> test_dbg("Test");

// ----------------------------------------------------------------------------
struct price
{
  double value;
  std::int64_t time;
};

// ----------------------------------------------------------------------------
void from_json(nlohmann::json const& j, price& p)
{
  if (j.size() == 2)
  {
    if (j[0] == nullptr)
      p.value = 0;
    else
      p.value = stod(j[0].get<std::string>());
    p.time = j[1];
    test_dbg<7>.debug(ffmt<s20>("price history"), msecs_unix_to_calendar_time_local(p.time * 1000));
  }
}

// ----------------------------------------------------------------------------
void handle_price_history(QByteArray byteArray)
{
  std::string_view data(byteArray.constData(), byteArray.length());
  nlohmann::json jdata = nlohmann::json::parse(data);
  test_dbg<9>.debug(ffmt<s20>("price history"), jdata.dump(4));
  counter++;
  //
  auto subsect = jdata["data"]["prices"]["all"]["prices"];
  test_dbg<8>.debug(ffmt<s20>("price history"), subsect.dump(4));
  auto prices = subsect.get<std::vector<price>>();
  auto first_date = prices.back().time;
  std::string first_string = msecs_unix_to_calendar_time_local(first_date * 1000);
  test_dbg<6>.debug(ffmt<s20>("First date"), first_string);
  //
  if (first_string != "2020-05-26 02:00:00")
  {
    test_dbg<6>.error(ffmt<s20>("Fail"), first_string, "2020-05-26 02:00:00");
    counter--;
  }
  QCoreApplication::quit();
}

// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  QCoreApplication a(argc, argv);
  QNetworkAccessManager networkmanager;
  //
  std::string url =
      fmt::format("https://{}:{}/api-internal/price-history/xrpgbp/", "www.bitstamp.net", 443);
  auto* client = net::http::qhttp_request_client::create(networkmanager, url);
  client->get_request(&handle_price_history);
  //
  a.exec();
  //
  return counter.load() > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
