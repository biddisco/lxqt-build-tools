#include <string>
//
#include <QMenu>
#include <QPlainTextEdit>
#include <QString>
//
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
//
#include <pika/execution/algorithms/just.hpp>
#include <pika/execution/algorithms/transfer_just.hpp>
//
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"
#include "exchange/bitstamp.hpp"
#include "exchange/xrpl_network.hpp"
#include "network/evp-encrypt.hpp"
#include "network/qhttp-request-client.hpp"
#include "senders/qhttp-post-sender.hpp"
#include "util/datetime_utils.hpp"
#include "util/json_qstring.hpp"
#include "util/stringutils.hpp"
#include "widgets/price_chart_widget.hpp"
#include "widgets/wallet_widget.hpp"
//
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockWidget.h"
//
#include <exec/inline_scheduler.hpp>
#include <exec/variant_sender.hpp>
#include <stdexec/execution.hpp>
//
#include "senders/qt_mainthread_scheduler.hpp"

// ----------------------------------------------------------------------------
using namespace grox;
using namespace grox::debug;
using namespace grox::senders;
using namespace nlohmann;
namespace ex = pika::execution::experimental;

// ----------------------------------------------------------------------------
template <int Level>
static print_threshold<Level, 3> bitstamp_dbg("Bitstamp");

// ----------------------------------------------------------------------------
std::atomic<bool> bitstamp_network::closing_down_ = false;
// ----------------------------------------------------------------------------
bitstamp_network::bitstamp_network()
{
  bitstamp_account default_acct;
  default_acct.name_ = "Bitstamp Main";
  accounts_.push_back(default_acct);
  using namespace std::literals;
  token_expiry_ = std::chrono::system_clock::now() - 60 * 1s;
  // update candles regularly
  connect(global_settings.get_global_clock_timer(), SIGNAL(timeout()), this,
    SLOT(candlestick_timer_event()), Qt::QueuedConnection);
  // after new data has been received, trigger this to process new candles and replot
  connect(this, SIGNAL(new_ohlc_data(ticker_data*, double)), this,
    SLOT(new_ohlc_data_event(ticker_data*, double)));

  connect(
    this, &bitstamp_network::new_live_trade_data_ui, this,
    [this](currency_pair cp, live_trades t) {
      auto p = t.price;
      auto v = t.amount;
      ohlctv_sample new_sample(1000.0 * std::atof(t.timestamp.c_str()), p, p, p, p, v);
      const ticker_data tdata = tickers_subscribed_.at(cp);
      tdata.view_->add_live_data(new_sample);
      tdata.chart_widget_->update_live_data(new_sample);
      // stream_process(new_sample);
    },
    Qt::QueuedConnection);
  //
  closing_down_ = false;
}

// ----------------------------------------------------------------------------
bitstamp_network::~bitstamp_network()
{
  bitstamp_dbg<0>.debug(str<>("destructor"));
}

// ----------------------------------------------------------------------------
// token is valid if it has more than (say) 5s of time left before it expires
bool token_valid(std::atomic<std::chrono::time_point<std::chrono::system_clock>>& expiry)
{
  using namespace std::literals;
  return ((expiry.load() - std::chrono::system_clock::now()) / 1s) > 5;
}

// ----------------------------------------------------------------------------
void bitstamp_network::initialize()
{
  auto web = stdexec::on(exec::inline_scheduler(), stdexec::just())                  // Qt
    | stdexec::let_value(std::bind(&bitstamp_network::get_websocket_token, this))    // Qt -> pika
    | stdexec::then([this](QByteArray byteArray) {                                   // pika
        std::string_view data(byteArray.constData(), byteArray.length());
        bitstamp_dbg<6>.debug(str<>("Initialize"), "WebsocketToken", data);
        handle_websocket_token(data);
      })                                                                               //
    | stdexec::transfer(grox::senders::qt_mainthread_scheduler())                      // pika -> Qt
    | stdexec::let_value(std::bind(&bitstamp_network::get_tickers_available, this))    // Qt -> pika
    | stdexec::then([this](QByteArray byteArray) {                                     // pika
        std::string_view data(byteArray.constData(), byteArray.length());
        bitstamp_dbg<6>.debug(str<>("Initialize"), "Tickers", data);
        handle_tickers_available(data);
      })                                                                          //
    | stdexec::transfer(grox::senders::qt_mainthread_scheduler())                 // pika -> Qt
    | stdexec::let_value(std::bind(&bitstamp_network::get_account_info, this))    // Qt -> pika
    | stdexec::then([this](QByteArray byteArray) {                                // pika
        std::string_view data(byteArray.constData(), byteArray.length());
        bitstamp_dbg<6>.debug(str<>("Initialize"), "AccountInfo", data);
        handle_account_info(data);
      })                                                                         //
    | stdexec::transfer(grox::senders::qt_mainthread_scheduler())                // pika -> Qt
    | stdexec::let_value(std::bind(&bitstamp_network::get_open_orders, this))    // Qt -> pika
    | stdexec::then([this](QByteArray byteArray) {                               // pika
        std::string_view data(byteArray.constData(), byteArray.length());
        bitstamp_dbg<6>.debug(str<>("Initialize"), "OpenOrders", data);
        handle_open_orders(data);
      })    //
    | stdexec::then([this]() { emit network_initialized(this); });

  // bitstamp_dbg<0>.debug(str<>("SENDER"), grox::debug::print_type<decltype(snd0)>());
  stdexec::start_detached(std::move(web));
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_live_trades(currency_pair const& cp, bool enable)
{
  std::string ticker = currency_pair_lowercase_string(cp);
  json command;
  command["event"] = enable ? "bts:subscribe" : "bts:unsubscribe";
  command["data"]["channel"] = string_join("live_trades_", ticker);

  ticker_data& tdata = tickers_subscribed_.at(cp);
  bitstamp_dbg<4>.debug(str<>("websocket trades"), command["event"],
    string_join("live_trades_", ticker), command.dump(4));

  if (enable)
  {
    using namespace std::placeholders;
    tdata.websockets_[network::streams::live_trades] = net::ws::qwebsocket_session::create(
      "bs::Trades " + ticker, bitstamp_websocket_address, bitstamp_websocket_port, command.dump(4),
      std::bind(bitstamp_network::new_live_trade_data_q, this, cp, _1));
  }
  else
  {
    tdata.websockets_[network::streams::live_trades].reset();
    tdata.websockets_.erase(network::streams::live_trades);
  }
  return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_order_book(currency_pair const& cp, bool enable)
{
  std::string ticker = currency_pair_lowercase_string(cp);
  json command;
  command["event"] = enable ? "bts:subscribe" : "bts:unsubscribe";
  command["data"]["channel"] = string_join("order_book_", ticker);

  ticker_data& tdata = tickers_subscribed_.at(cp);
  bitstamp_dbg<4>.debug(str<>("websocket orders"), command["event"],
    string_join("order_book_", ticker), command.dump(4));

  if (enable)
  {
    using namespace std::placeholders;
    tdata.websockets_[network::streams::order_book] = net::ws::qwebsocket_session::create(
      "bs::Orders " + ticker, bitstamp_websocket_address, bitstamp_websocket_port, command.dump(4),
      std::bind(&bitstamp_network::new_orderbook_data_q, this, cp, _1));
  }
  else
  {
    tdata.websockets_[network::streams::order_book].reset();
    tdata.websockets_.erase(network::streams::order_book);
  }
  return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_my_trades(currency_pair const& cp, bool enable)
{
  std::string ticker = currency_pair_lowercase_string(cp);
  json command;
  command["event"] = enable ? "bts:subscribe" : "bts:unsubscribe";
  command["data"]["channel"] = string_join("private-my_trades_", ticker) + "-" + websocket_user_id_;
  command["data"]["auth"] = websocket_token_;

  ticker_data& tdata = tickers_subscribed_.at(cp);
  bitstamp_dbg<3>.debug(str<>("websocket mytrades"), command["event"],
    string_join("private-my_trades_", ticker), command.dump(4));

  if (enable)
  {
    tdata.websockets_[network::streams::my_trades] =
      net::ws::qwebsocket_session::create("bs::MyTrades " + ticker, bitstamp_websocket_address,
        bitstamp_websocket_port, command.dump(4), [](const QString data) {
          //
          bitstamp_dbg<7>.debug(str<>("(private) Trade data handler"), data);
        });
  }
  else
  {
    tdata.websockets_[network::streams::my_trades].reset();
    tdata.websockets_.erase(network::streams::my_trades);
  }
  return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_my_orders(currency_pair const& cp, bool enable)
{
  std::string ticker = currency_pair_lowercase_string(cp);
  json command;
  command["event"] = enable ? "bts:subscribe" : "bts:unsubscribe";
  command["data"]["channel"] = string_join("private-my_orders_", ticker) + "-" + websocket_user_id_;
  command["data"]["auth"] = websocket_token_;

  ticker_data& tdata = tickers_subscribed_.at(cp);
  bitstamp_dbg<3>.debug(str<>("websocket myorders"), command["event"],
    string_join("private-my_orders_", ticker), command.dump(4));

  if (enable)
  {
    tdata.websockets_[network::streams::my_orders] =
      net::ws::qwebsocket_session::create("bs::MyOrders " + ticker, bitstamp_websocket_address,
        bitstamp_websocket_port, command.dump(4), [this](const QString data) {
          std::string stdstring = data.toStdString();
          bitstamp_dbg<7>.debug(str<>("Orders data"), stdstring);
          json jdata = json::parse(stdstring);
          if (jdata["event"] == "bts:subscription_succeeded")
          {
            bitstamp_dbg<0>.debug(str<>("Orders data"), "bts:subscription_succeeded");
          }
          else
          {
            process_order(jdata["data"], jdata["event"].get<std::string_view>());
          }
        });
  }
  else
  {
    // tdata.websocket_->write(command.dump(4));
    tdata.websockets_[network::streams::my_orders].reset();
    tdata.websockets_.erase(network::streams::my_orders);
  }
  return true;
}

// ----------------------------------------------------------------------------
// connect to a single stream
bool bitstamp_network::stream_subscribe(
  currency_pair const& cp, network::streams const stream, bool enabled)
{
  get_websocket_token();
  //
  bool ok = true;
  switch (stream)
  {
  case network::streams::my_trades:
    ok = subscribe_my_trades(cp, enabled);
    break;
  case network::streams::my_orders:
    ok = subscribe_my_orders(cp, enabled);
    break;
  case network::streams::live_trades:
    ok = subscribe_live_trades(cp, enabled);
    break;
  case network::streams::order_book:
    ok = subscribe_order_book(cp, enabled);
    break;
  default:
    ok = false;
    throw std::runtime_error("unknown stream");
  }
  if (ok)
    mark_stream_subscribed(currency_pair_string(cp) + "/" + stream_to_text(stream), enabled);
  return ok;
}

// ----------------------------------------------------------------------------
void bitstamp_network::shut_down()
{
  // do not allow shutdown / async operations concurrently
  closing_down_ = true;
  std::lock_guard l(async_mutex_);
  //
  bitstamp_dbg<0>.debug(str<>("websockets"), "shutdown start");
  //
  for (auto& [ticker, tdata] : tickers_subscribed_)
  {
    for (auto& [stream, websocket] : tdata.websockets_)
    {
      try
      {
        bitstamp_dbg<2>.debug(
          str<>("websocket close"), currency_pair_string(ticker), ptr(websocket.get()));
        websocket.reset();
      }
      catch (const std::exception& err)
      {
        std::cerr << err.what() << std::endl;
      }
    }
    // delete orderbook _after_ closing websocket to avoid some late async data arrivals
    delete tdata.orderbook_;
  }
  tickers_subscribed_.clear();
}

// ----------------------------------------------------------------------------
bitstamp_order_book const& bitstamp_network::get_orderbook(currency_pair const& cp) const
{
  const ticker_data tdata = tickers_subscribed_.at(cp);
  return *dynamic_cast<bitstamp_order_book*>(tdata.orderbook_);
}

// ----------------------------------------------------------------------------
bool bitstamp_network::can_send(const currency& c, exchange* dest)
{
  auto xrp_net = dynamic_cast<xrpl_network*>(dest);
  if (xrp_net && !xrp_net->testnet())
  {
    if (c.is_xrp() && (c.issuer_ == currency::bitstamp_trust) &&
      ((c.code_ == "USD") || (c.code_ == "EUR")))
      return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
double bitstamp_network::get_fee_percent(currency const& c1, currency const& c2)
{
  std::pair<std::string, std::string> cpair;
  if (c1.is_xrp())
  {
    cpair = std::make_pair(lowercase(c1.to_string().first), lowercase(c2.to_string().first));
  }
  else
  {
    cpair = std::make_pair(lowercase(c2.to_string().first), lowercase(c1.to_string().first));
  }
  const auto val = fee_map_.at(cpair);
  return val;
}

// ----------------------------------------------------------------------------
double bitstamp_network::get_fee_fixed(currency const& c1, currency const& c2)
{
  return 0.0;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::make_payment(currency const& c, basic_account* src, basic_account* dest)
{
  bitstamp_account* from = static_cast<bitstamp_account*>(src);
  ledger_wallet* to = static_cast<ledger_wallet*>(dest);
  bitstamp_dbg<0>.debug(str<>("make_payment"), "amount", c.balance_, "currency", c, "from",
    from->name_, "to", to->public_, ((to->tag_ != 0) ? "(" + std::to_string(to->tag_) + ")" : ""));

  std::stringstream req_string;
  req_string << "amount=" << c.balance_ << "&address=" << to->public_;

  // issue a withdrawal payment to the wallet
  if (c.is_xrp())
  {
    req_string << "&destination_tag"
               << "PUT SOMETHING IN HERE";
    //
    auto* client = signed_request("/api/v2/xrp_withdrawal/", req_string.str());
    auto web = stdexec::on(exec::inline_scheduler(), stdexec::just())          // Qt
      | stdexec::let_value(std::move(stdexec::just(client) | qhttp_post()))    // Qt -> pika
      | stdexec::then([this](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          bitstamp_dbg<0>.debug(str<>("request CB"), "/api/v2/xrp_withdrawal/", data);
          emit transaction_event();
        });
  }
  // this is an IOU transfer
  else
  {
    req_string << "&currency= this is wrong" << c.issuer_;
    //
    auto* client = signed_request("/api/v2/ripple_withdrawal/", req_string.str());
    auto web = stdexec::on(exec::inline_scheduler(), stdexec::just())          // Qt
      | stdexec::let_value(std::move(stdexec::just(client) | qhttp_post()))    // Qt -> pika
      | stdexec::then([this](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          bitstamp_dbg<0>.debug(str<>("request CB"), "/api/v2/ripple_withdrawal/", data);
          emit transaction_event();
        });
  }

  return true;
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::get_account_info()
{
  auto* client = signed_request("/api/v2/balance/", "");
  return any_bytearray_sender{stdexec::just(client) | qhttp_post()};
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::get_websocket_token()
{
  if (token_valid(token_expiry_))
  {
    bitstamp_dbg<2>.debug(str<>("websocket_token"), "Still valid");
    return any_bytearray_sender{stdexec::just_stopped()};
  }
  //
  bitstamp_dbg<2>.debug(str<>("websocket_token"), "Fetching new");
  auto* client = signed_request("/api/v2/websockets_token/", "");
  return any_bytearray_sender{stdexec::just(client) | qhttp_post()};
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::get_open_orders()
{
  auto* client = signed_request("/api/v2/open_orders/all/", "");
  return any_bytearray_sender{stdexec::just(client) | qhttp_post()};
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::get_tickers_available()
{
  std::string url = fmt::format("https://{}:{}{}", bitstamp_https_address, 443, "/api/v2/ticker/");
  auto* client = net::http::qhttp_request_client::create(*global_settings.networkmanager_, url);
  return any_bytearray_sender{
    std::move(stdexec::just(client) | qhttp_post(grox::senders::http_request_type::http_get))};
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_account_info(std::string_view data)
{
  json jdata = json::parse(data);
  bitstamp_dbg<6>.debug(str<>("account info"), jdata.dump(4));
  //
  bitstamp_account& acct = get_bitstamp_instance()->account();

  currency xrp_bitstamp{{"", "XRP"},
    std::stod(jdata["xrp_balance"].get_ptr<json::string_t*>()->c_str()),
    std::stod(jdata["xrp_available"].get_ptr<json::string_t*>()->c_str()),
    std::stod(jdata["xrp_reserved"].get_ptr<json::string_t*>()->c_str()), nullptr};
  acct.add_currency(xrp_bitstamp);

  if (jdata.contains("usd_balance"))
  {
    currency usd_bitstamp{{currency::bitstamp_trust, "USD"},
      std::stod(jdata["usd_balance"].get_ptr<json::string_t*>()->c_str()),
      std::stod(jdata["usd_available"].get_ptr<json::string_t*>()->c_str()),
      std::stod(jdata["usd_reserved"].get_ptr<json::string_t*>()->c_str()), nullptr};
    acct.add_currency(usd_bitstamp);
  }

  if (jdata.contains("eur_balance"))
  {
    currency eur_bitstamp{{currency::bitstamp_trust, "EUR"},
      std::stod(jdata["eur_balance"].get_ptr<json::string_t*>()->c_str()),
      std::stod(jdata["eur_available"].get_ptr<json::string_t*>()->c_str()),
      std::stod(jdata["eur_reserved"].get_ptr<json::string_t*>()->c_str()), nullptr};
    acct.add_currency(eur_bitstamp);
  }

  if (jdata.contains("xrpusd_fee"))
  {
    double xrpusd_fee = std::stod(jdata["xrpusd_fee"].get_ptr<json::string_t*>()->c_str());
    std::pair<std::string, std::string> cpair = std::make_pair("xrp", "usd");
    const auto [it, success] = fee_map_.insert({cpair, xrpusd_fee});
    if (success)
    {
      bitstamp_dbg<0>.debug(str<>("new fee xrp/usd"), xrpusd_fee);
    }
    else
    {
      bitstamp_dbg<0>.debug(str<>("replace fee xrp/usd"), xrpusd_fee);
      fee_map_[cpair] = xrpusd_fee;
    }
  }

  if (jdata.contains("xrpeur_fee"))
  {
    double xrpeur_fee = std::stod(jdata["xrpeur_fee"].get_ptr<json::string_t*>()->c_str());
    std::pair<std::string, std::string> cpair = std::make_pair("xrp", "eur");
    const auto [it, success] = fee_map_.insert({cpair, xrpeur_fee});
    if (success)
    {
      bitstamp_dbg<0>.debug(str<>("new fee xrp/eur"), xrpeur_fee);
    }
    else
    {
      bitstamp_dbg<0>.debug(str<>("replace fee xrp/eur"), xrpeur_fee);
      fee_map_[cpair] = xrpeur_fee;
    }
  }
  emit update_wallet_widget(&acct);
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_websocket_token(std::string_view data)
{
  json jdata = json::parse(data);
  bitstamp_dbg<5>.debug(str<>("websocket token"), jdata.dump());
  //
  bitstamp_account& acct = get_bitstamp_instance()->account();
  //
  using namespace std::literals;
  auto valid_sec = jdata["valid_sec"].get<int>();
  websocket_token_ = jdata["token"].get<std::string>();
  websocket_user_id_ = std::to_string(jdata["user_id"].get<int>());
  token_expiry_ = std::chrono::system_clock::now() + valid_sec * 1s;

  bitstamp_dbg<2>.debug(str<>("websocket_token"),
    token_valid(token_expiry_) ? "valid until" : "expired",
    fmt::format("{:%Y-%m-%d %X}", round<std::chrono::seconds>(token_expiry_.load())));
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_open_orders(std::string_view data)
{
  bitstamp_account& acct = get_bitstamp_instance()->account();
  auto& trades = acct.offers_;
  trades.clear();
  //
  json jdata = json::parse(data);
  for (auto const& [key, val] : jdata.items())
  {
    const std::string jstring = val[std::string_view("currency_pair")];
    auto const& [c1, c2] = split_currency_pair_string(jstring, '/');

    double amount = std::stod(JCHARP(val["amount"]));
    double price = std::stod(JCHARP(val["price"]));
    double fee_percent = 0;
    double fee_fixed = 0;
    // 0=buy, 1=sell
    auto trade_type_ = (val["type"] == "0") ? trade_type::buy : trade_type::sell;

    if (trade_type_ == trade_type::sell)
    {
      trade_data t{this->get_instance(), account().name_, {"", c2.cbegin()}, {"", c1.cbegin()},
        amount * price, amount, price, fee_percent, fee_fixed,
        std::stoull(val["id"].get<std::string>()), val["datetime"], true};
      trades.push_back(t);
    }
    else
    {
      trade_data t{this->get_instance(), account().name_, {"", c1.cbegin()}, {"", c2.cbegin()},
        amount, amount * price, price, fee_percent, fee_fixed,
        std::stoull(val["id"].get<std::string>()), val["datetime"], true};
      trades.push_back(t);
    }
  }
  emit update_wallet_widget(&acct);
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_tickers_available(std::string_view data)
{
  json jdata = json::parse(data);
  for (auto const& [key, val] : jdata.items())
  {
    json::string_t jstring = val[std::string_view("pair")];
    auto const& [c1, c2] = string_to_pair(jstring, "/");
    bitstamp_dbg<6>.debug(str<>("Currency pair"), jstring, c1, c2);
    add_currency_pair(c1, c2);
  }
}

// ----------------------------------------------------------------------------
/* A typical order will have the following structure

{
  "data": {
    "id": 1552110926278656,
    "id_str": "1552110926278656",
    "order_type": 1,
    "datetime": "1667768306",
    "microtimestamp": "1667768306268000",
    "amount": 1000,
    "amount_str": "1000.00000000",
    "price": 0.54,
    "price_str": "0.54000"
  },
  "channel": "private-my_orders_xrpusd-1227955",
  "event": "order_created"
}
{
  "data": {
    "id": 1552274932695043,
    "id_str": "1552274932695043",
    "order_type": 1,
    "datetime": "1667808347",
    "microtimestamp": "1667808346897000",
    "amount": 1000,
    "amount_str": "1000.00000000",
    "price": 0.54,
    "price_str": "0.54000"
  },
  "channel": "private-my_orders_xrpusd-1227955",
  "event": "order_created"
}

*/
void bitstamp_network::process_order(json& jdata, std::string_view event)
{
  bitstamp_account& acct = get_bitstamp_instance()->account();
  auto& trades = acct.offers_;
  //
  std::uint64_t id = jdata["id"];
  auto find_by_id = [id](trade_data& t) { return t.id_ == id; };

  if (event == "order_deleted")
  {
    auto trade = std::find_if(trades.begin(), trades.end(), find_by_id);
    if (trade == trades.end())
    {
      bitstamp_dbg<0>.error(str<>("Order not found"), dec<18>(id));
    }
    else
    {
      bitstamp_dbg<0>.debug(str<>("Order deleted"), dec<18>(id));
      trades.erase(trade);
    }
  }
  if (event == "order_created")
  {
    auto trade = std::find_if(trades.begin(), trades.end(), find_by_id);
    if (trade == trades.end())
    {
      bitstamp_dbg<0>.error(str<>("Order not found"), dec<18>(id));
    }
    else
    {
      if (trade->confirmed_ == false)
      {
        bitstamp_dbg<0>.debug(str<>("Order created"), dec<18>(id), "confirmed");
        trade->confirmed_ = true;
      }
      else
      {
        bitstamp_dbg<0>.error(str<>("Order created"), dec<18>(id), "already active");
      }
    }
  }
  emit update_wallet_widget(&acct);
}

/* A list of open orders take the form
[
  {
    "id": "1552120149377025",
    "datetime": "2022-11-06 21:35:58",
    "type": "1",
    "amount": "50000.00000000",
    "price": "0.54200",
    "amount_at_create": "50000.00000000",
    "currency_pair": "XRP/USD"
  }
]

*/

// ----------------------------------------------------------------------------
net::http::client_ptr bitstamp_network::signed_request(
  const std::string& url_path, const std::string& url_query)
{
  std::string api_key = get_bitstamp_instance()->account().API_key;
  std::string api_secret = get_bitstamp_instance()->account().API_secret;
  secure_string randbytes = generate_random_alphanumeric_string(encryption::KEY_SIZE, 81192);
  encryption encryptor(api_key, randbytes);
  //
  std::chrono::milliseconds timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch());

  // setup REST request fields
  std::string url_host = bitstamp_https_address;
  std::string content_type = "application/x-www-form-urlencoded";
  std::string payload = url_query.size() > 0 ? url_query : url_encode("{offset:1}");
  std::string http_method = "POST";
  std::string x_auth = "BITSTAMP " + api_key;
  std::string x_auth_nonce = encryptor.generate_uuid_string();
  std::string x_auth_timestamp = std::to_string(timestamp.count());
  std::string x_auth_version = "v2";

  // https://www.bitstamp.net/api/#section/Authentication
  // x_auth_signature:
  //   sha256.hmac({string_to_sign}, {api_secret})
  //   {string_to_sign} is your signature message.
  //   Content-Type should not be added to the string if request.body is empty.
  //   The following have to be combined into a single string:
  //   "BITSTAMP" + " " + api_key +
  //   HTTP Verb +
  //   url.host +
  //   url.path +
  //   url.query +
  //   Content-Type +
  //   X-Auth-Nonce +
  //   X-Auth-Timestamp +
  //   X-Auth-Version +
  //   request.body

  std::string string_to_sign = "";
  string_to_sign.append(x_auth);
  string_to_sign.append(http_method);
  string_to_sign.append(url_host);
  string_to_sign.append(url_path);
  string_to_sign.append(url_query);
  string_to_sign.append(payload.size() > 0 ? content_type.c_str() : "");
  string_to_sign.append(x_auth_nonce);
  string_to_sign.append(x_auth_timestamp);
  string_to_sign.append(x_auth_version);
  string_to_sign.append(payload);

  // generated signature
  auto signed_hmac = encryptor.CalcHmacSHA256(api_secret, string_to_sign);
  assert(signed_hmac.size() == 32);
  std::string x_auth_signature = b2a_hex(signed_hmac.data(), signed_hmac.size());

  std::string urlstring = fmt::format(
    "https://{}:{}{}{}", bitstamp_https_address, bitstamp_https_port, url_path, url_query);
  bitstamp_dbg<7>.debug(str<>("account_request"), urlstring, string_to_sign);

  QNetworkRequest request(QUrl(urlstring.c_str()));
  request.setRawHeader("Content-Type", content_type.c_str());
  request.setRawHeader("User-Agent", "mystery");
  request.setRawHeader("Accept", "application/json");
  request.setRawHeader("Connection", "close");
  //
  request.setRawHeader("X-Auth", x_auth.c_str());
  request.setRawHeader("X-Auth-Signature", x_auth_signature.c_str());
  request.setRawHeader("X-Auth-Nonce", x_auth_nonce.c_str());
  request.setRawHeader("X-Auth-Timestamp", x_auth_timestamp.c_str());
  request.setRawHeader("X-Auth-Version", x_auth_version.c_str());

  auto* client = net::http::qhttp_request_client::create_signed(
    *global_settings.networkmanager_, request, std::move(payload));
  return client;
}

// ----------------------------------------------------------------------------
void bitstamp_network::new_orderbook_data_q(
  bitstamp_network* n, currency_pair const cp, const QString data)
{
  bitstamp_dbg<5>.debug(str<>("Orderbook"), "Ticker", currency_pair_string(cp));
  bitstamp_dbg<7>.debug(str<>("Orderbook data"), data);
  //
  std::lock_guard l(n->async_mutex_);
  if (closing_down_)
  {
    bitstamp_dbg<0>.error(str<>("Orderbook data"), "Shutdown in progress: ignoring data");
    return;
  }
  //
  const ticker_data tdata = n->tickers_subscribed_.at(cp);
  //
  if (!tdata.orderbook_)
    return;    // @todo probably called during destruction

  if (!dynamic_cast<bitstamp_order_book*>(tdata.orderbook_)->accept_json_bitstamp(data))
    return;

  emit n->orderbook_changed();
}

// ----------------------------------------------------------------------------
void bitstamp_network::new_live_trade_data_q(
  bitstamp_network* n, currency_pair cp, const QString data)
{
  bitstamp_dbg<4>.debug(str<>("Live Trade"), "Ticker", currency_pair_string(cp));
  bitstamp_dbg<5>.debug(str<>("Trade data"), data);
  //
  std::lock_guard l(n->async_mutex_);
  if (closing_down_)
  {
    bitstamp_dbg<0>.error(str<>("trade data"), "Shutdown in progress: ignoring data");
    return;
  }
  if (!startswith(data, "{\"data\":"))
    return;
  //
  std::string stdstring = data.toStdString();
  //
  json jdata = json::parse(stdstring);
  // extract the main subgroup
  jdata = jdata["data"];
  bitstamp_dbg<7>.debug(str<>("Trade data parsed"), jdata.dump(4));
  live_trades trade_data = jdata.get<live_trades>();
  //
  emit n->new_live_trade_data_ui(cp, trade_data);
}

// ----------------------------------------------------------------------------
// OHLC candlestick updates
// ----------------------------------------------------------------------------
// typically called once per minute by the application to update data regularly
void bitstamp_network::update_ohlc_datasets()
{
  for (auto& [ticker, data] : tickers_subscribed_)
  {
    if (!candlestick_updates_active_.contains(ticker))
    {
      update_ohlc_data(ticker, &data);
    }
  }
}

// ----------------------------------------------------------------------------
void bitstamp_network::update_ohlc_data(currency_pair cp, ticker_data* tdata)
{
  // does not matter if ticker already in map, it will be cleaned out when done
  candlestick_updates_active_.insert(cp);
  //
  uint64_t req_t = 0, start_t_ms = 0;
  // what is the most recent sample we currently have
  start_t_ms = static_cast<uint64_t>(tdata->view_->get_last_sample_time(false));
  if (start_t_ms == 0)
  {
    // if no data exists, we use an arbitrary start date in 2017
    // linux time 1496275200 = Thu Jun 01 2017 00:00:00 GMT+0000
    start_t_ms = 1496275200ul * 1000;
    std::string s = msecs_unix_to_calendar_time(start_t_ms);
    bitstamp_dbg<0>.debug(
      str<>("No Data"), tdata->view_->get_ticker_string(), "requesting from", s);
  }
  else
  {
    std::string s = msecs_unix_to_calendar_time(start_t_ms);
    bitstamp_dbg<0>.debug(str<>("data ok up to"), tdata->view_->get_ticker_string(), s);
  }

  // we do not want a candle for the current minute, so subtract 60s
  QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
  uint64_t unixtime_secs = currentDateTime.toSecsSinceEpoch() - 60;
  //
  uint64_t diff = unixtime_secs - (start_t_ms / 1000);
  uint64_t samples = diff / 60;
  if (samples == 0)
  {
    bitstamp_dbg<0>.debug(str<>("candlesticks"), tdata->view_->get_ticker_string(), "up to date");
    candlestick_updates_active_.erase(cp);
    return;
  }

  // convert to unix timestamp : next sample is 60s after last
  req_t = (start_t_ms / 1000) + 60;

  bitstamp_dbg<0>.debug(str<>("requesting"), tdata->view_->get_ticker_string(), samples,
    msecs_unix_to_calendar_time(req_t * 1000));

  auto snd = stdexec::on(grox::senders::qt_mainthread_scheduler(),
               request_new_ohlc_data(cp, req_t, samples)) |
    stdexec::then([this, req_t, cp, tdata](QByteArray byteArray) {
      std::string_view data(byteArray.constData(), byteArray.length());
      bitstamp_dbg<0>.debug(str<>("OHLC (lambda)"), tdata->view_->get_ticker_string(),
        msecs_unix_to_calendar_time(req_t * 1000));
      handle_new_ohlc_data(tdata, data);
    }) |
    stdexec::then([this, cp, tdata]() { this->update_ohlc_data(cp, tdata); });

  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
// generate an http request for candlestick data for a single ticker
any_bytearray_sender bitstamp_network::request_new_ohlc_data(
  currency_pair cp, uint64_t start_t, uint64_t samples)
{
  std::string ticker_lowercase = currency_pair_lowercase_string(cp);
  // the caller must have set the update flag already
  if (!candlestick_updates_active_.contains(cp))
  {
    throw std::runtime_error("Update candlestick without map set");
  }

  std::string req;
  if (start_t == 0)
  {
    req = fmt::format("/api/v2/ohlc/{}/?step=60&limit=1000", ticker_lowercase);
  }
  else
  {
    if (samples >= 1000)
    {
      bitstamp_dbg<5>.debug(str<>("Limiting request"), samples);
      samples = 1000;
    }
    std::string start = std::to_string(start_t);
    std::string limit = std::to_string(samples);
    // send a request for ticker data using the io context thread to make the request
    req = fmt::format("/api/v2/ohlc/{}/?step=60&start={}&limit={}", ticker_lowercase, start, limit);
    bitstamp_dbg<0>.debug(str<>("request"), ticker_lowercase, req);
  }

  // @todo : add error hander
  std::string url = fmt::format("https://{}:{}{}", bitstamp_https_address, 443, req);
  net::http::client_ptr client =
    net::http::qhttp_request_client::create(*global_settings.networkmanager_, url);
  return {stdexec::just(client) | qhttp_post(grox::senders::http_request_type::http_get)};
}

// ----------------------------------------------------------------------------
void bitstamp_network::cancel_order(trade_data const& t)
{
  std::string data = "&id=" + std::to_string(t.id_);

  auto* client = signed_request("/api/v2/cancel_order/", data);
  auto web = stdexec::on(exec::inline_scheduler(), stdexec::just())          // Qt
    | stdexec::let_value(std::move(stdexec::just(client) | qhttp_post()))    // Qt -> pika
    | stdexec::then([this](QByteArray byteArray) {
        std::string_view data(byteArray.constData(), byteArray.length());
        json jdata = json::parse(data);
        bitstamp_dbg<0>.debug(str<>("Cancel Order response"), jdata.dump(4));
        // refresh order status
        throw std::runtime_error("Fix this websocket changed");
        //    if (ws_myorders == nullptr)
        {
          get_open_orders();
        }
      });
}

// ----------------------------------------------------------------------------
void bitstamp_network::place_limit_order(trade_data const& t, bool update_after)
{
  double amount = t.get_xrp_amount();

  // bitstamp trade pair is always xrpusd, so swap symbols accordingly
  bitstamp_dbg<0>.error(str<>("limit-order"), "@TODO USD assumption false");
  std::string req, data;
  if (t.get_trade_type() == trade_type::buy)
  {
    req = std::string("/api/v2/buy/") + std::string(t.taker_payc_.to_string().first) +
      std::string(t.taker_getc_.to_string().first) + "/";
    data = "&amount=" + to_string(amount, t.taker_payc_) +
      "&price=" + to_string_with_precision(t.exchange_rate_, 5);
  }
  else
  {
    req = std::string("/api/v2/sell/") + std::string(t.taker_getc_.to_string().first) +
      std::string(t.taker_payc_.to_string().first) + "/";
    data = "&amount=" + to_string(amount, t.taker_getc_) +
      "&price=" + to_string_with_precision(t.exchange_rate_, 5);
  }
  // make lowercase "XRPUSD"->"xrpusd" for bitstamp API
  lowercase_i(req);
  //
  bitstamp_dbg<0>.debug(
    str<>("limit-order"), (t.get_trade_type() == trade_type::buy ? "Buy" : "Sell"), req, data);

  auto* client = signed_request(req, data);
  auto web = stdexec::on(exec::inline_scheduler(), stdexec::just())          // Qt
    | stdexec::let_value(std::move(stdexec::just(client) | qhttp_post()))    // Qt -> pika
    | stdexec::then([=, this](QByteArray byteArray) {
        std::string_view data(byteArray.constData(), byteArray.length());
        json jdata = json::parse(data);
        bitstamp_dbg<0>.debug(str<>("limit-order response"), jdata.dump(4));
        // refresh order status if we don't have orders websocket
        throw std::runtime_error("Fix this websocket changed");
        if (update_after /*&& ws_myorders == nullptr*/)
        {
          get_open_orders();
        }
        if (jdata.contains("id"))
        {
          trade_data new_t = t;
          new_t.id_ = std::stoll(jdata["id"].get_ptr<json::string_t*>()->c_str());
          new_t.confirmed_ = true;
          new_t.datetime_ = jdata["datetime"].get_ptr<json::string_t*>()->c_str();
          double price = std::stod(jdata["price"].get_ptr<json::string_t*>()->c_str());
          double amount = std::stod(jdata["amount"].get_ptr<json::string_t*>()->c_str());
          if (price != new_t.get_price())
          {
            bitstamp_dbg<0>.error(str<>("limit-order price"), price, new_t.get_price());
          }
          auto& trades = account().offers_;
          trades.push_back(new_t);
          emit update_wallet_widget(&account());
        }
      });
}

// ----------------------------------------------------------------------------
void bitstamp_network::place_buy_sell_orders(
  basic_account* acct, std::vector<trade_data> const& trades)
{
  for (auto& t : trades)
  {
    if (&t != &trades.back())
      place_limit_order(t, false);
    else
      place_limit_order(t, true);
  }
}

// ----------------------------------------------------------------------------
void bitstamp_network::ticker_subscribe(currency const& c1, currency const& c2)
{
  // exit if this exchange has already subscribed to this ticker
  std::string cps = currency_pair_string({c1, c2});
  if (ticker_subscribed(c1, c2))
  {
    bitstamp_dbg<0>.debug(str<>("subscription"), cps, "subscribed");
    return;
  }
  bitstamp_dbg<0>.debug(str<>("subscribing"), cps);
  currency_pair cp{c1, c2};

  // create a new data view from hdf5
  std::shared_ptr<ohlc_dataset_view> view = std::make_shared<ohlc_dataset_view>("bitstamp", c1, c2);

  // create a new price plot object
  auto* chart_widget = new price_chart_widget(nullptr, view, shared_from_this(), cps);

  // put the price plot into a dock widget
  using namespace ads;
  std::string title = cps + " price " + std::string(name());
  CDockWidget* PlotDockWidget = new CDockWidget(QString(title.c_str()));
  PlotDockWidget->setWidget(chart_widget);
  PlotDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  global_settings.dock_manager_->addDockWidget(DockWidgetArea::LeftDockWidgetArea, PlotDockWidget);
  global_settings.dockwindows_menu_->addAction(PlotDockWidget->toggleViewAction());

  // create a new orderbook text display
  const size_t font_size = 8;
  auto* orderbook_text = new QPlainTextEdit(nullptr);
  QString txt = "X";
  int char_size = QFontMetrics(orderbook_text->font()).horizontalAdvance(txt);
  int calcWidth = char_size * 85 + 8;
  orderbook_text->setMinimumWidth(calcWidth);
  QFont font = QFont();
  font.setPointSize(font_size);
  font.setFamily("Courier");
  orderbook_text->setFont(font);

  // put the order book into a dock widget
  using namespace ads;
  std::string obtitle = cps + " text " + std::string(name());
  CDockWidget* obPlotDockWidget = new CDockWidget(QString(obtitle.c_str()));
  obPlotDockWidget->setWidget(orderbook_text);
  obPlotDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  global_settings.dock_manager_->addDockWidget(
    DockWidgetArea::LeftDockWidgetArea, obPlotDockWidget);
  global_settings.dockwindows_menu_->addAction(obPlotDockWidget->toggleViewAction());

  // ----------------------------------
  // Create orderbook plot widget
  OrderBookPlot* orderbook_plot = new OrderBookPlot();
  orderbook_plot->setMinimumSize(384, 256);
  //
  std::string obptitle = cps + " depth " + std::string(name());
  CDockWidget* obpDockWidget = new CDockWidget(QString(obptitle.c_str()));
  obpDockWidget->setWidget(orderbook_plot);
  obpDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  global_settings.dock_manager_->addDockWidget(DockWidgetArea::CenterDockWidgetArea, obpDockWidget);
  global_settings.dockwindows_menu_->addAction(obpDockWidget->toggleViewAction());

  bitstamp_order_book* orderbook = new bitstamp_order_book(orderbook_plot, false);
  // add the subscribed ticker/data/plot to our list for tracking
  tickers_subscribed_.insert({cp, {view, chart_widget, orderbook, orderbook_text, orderbook_plot}});

  connect(
    this, &bitstamp_network::orderbook_changed, this,
    [orderbook_plot, orderbook_text, cp, this]() {
      QString datastring = QString::fromStdString(get_orderbook(cp).order_text);
      orderbook_text->setPlainText(datastring);
      orderbook_plot->update_time_and_replot();
    },
    Qt::QueuedConnection);

  // start by displaying 1 day of data
  chart_widget->graph_rescale(0);
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_new_ohlc_data(ticker_data* tdata, std::string_view data)
{
  try
  {
    // convert json data into vectors of actual data
    json jdata = json::parse(data)["data"]["ohlc"];
    bitstamp_dbg<0>.debug(str<>("OHLC received"), tdata->view_->get_ticker_string(),
      dec<4>(jdata.size()), "json OHLC samples");
    QVector<ohlctv_sample> new_ohlc_samples;
    new_ohlc_samples.reserve(jdata.size());
    ohlctv_sample sample;
    for (auto item : jdata)
    {
      sample.close = atof(item["close"].get_ptr<json::string_t*>()->c_str());
      sample.high = atof(item["high"].get_ptr<json::string_t*>()->c_str());
      sample.low = atof(item["low"].get_ptr<json::string_t*>()->c_str());
      sample.open = atof(item["open"].get_ptr<json::string_t*>()->c_str());
      sample.time = atof(item["timestamp"].get_ptr<json::string_t*>()->c_str()) * 1000;
      sample.volume = atof(item["volume"].get_ptr<json::string_t*>()->c_str());
      new_ohlc_samples.push_back(sample);
    }
    //
    bitstamp_dbg<5>.debug(str<>("Converted"), tdata->view_->get_ticker_string(),
      new_ohlc_samples.size(), "new OHLC samples");
    tdata->view_->merge_data(ohlc_data_resolutions::minute, new_ohlc_samples);
    // what is the last sample we currently have
    auto last_time = tdata->view_->get_last_sample_time(false);
    bitstamp_dbg<0>.debug(str<>("data merged up to"), tdata->view_->get_ticker_string(),
      msecs_unix_to_calendar_time(last_time));
    tdata->view_->delete_live_data_up_to(last_time);
    tdata->chart_widget_->update();
    //
    emit new_ohlc_data(tdata, ohlc_data_resolutions::minute);
  }
  catch (std::exception& e)
  {
    bitstamp_dbg<0>.error(str<>("JSON error"), "decoding OHLC data:", e.what(), "\n", data, "\n\n");
    std::terminate();
  }
}

// ----------------------------------------------------------------------------
void bitstamp_network::new_ohlc_data_event(ticker_data* tdata, double old_res)
{
  (void) (old_res);
  bitstamp_dbg<5>.debug(str<>("new ohlc data"), "resolution", old_res);
  //
  // get all available candle resolutions, except highest res
  // since we we use that one to generate all the others
  auto const& resolutions = ohlc_data_resolutions::available_resolutions();
  for (size_t i = 1; i < resolutions.size(); ++i)
  {
    auto const& res = resolutions[i];
    auto data = tdata->view_->get_dataset(res);
    if (data)
    {
      data->resample_update(res, tdata->view_->get_dataset(res.base_),
        ohlc_data_resolutions::get_resolution(res.base_));
    }
    else
    {
      data = tdata->view_->get_dataset(res.base_)->resample(
        res, ohlc_data_resolutions::get_resolution(res.base_));
      tdata->view_->add_dataset(res, data);
    }
  }

  // don't change axes, just update data series and replot
  tdata->chart_widget_->replot();
}

// the bitstamp minute candle only updates around
// 8 seconds after the minute has ended, so ignore timer until then
// ----------------------------------------------------------------------------
void bitstamp_network::candlestick_timer_event()
{
  static int last_minute = -1;
  //
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  time_t tt = std::chrono::system_clock::to_time_t(now);
  // UTC! for local use # tm local_tm = *localtime(&tt);
  tm utc_tm = *gmtime(&tt);
  //
  if ((last_minute == -1) || ((last_minute != utc_tm.tm_min) && (utc_tm.tm_sec >= 8)))
  {
    last_minute = utc_tm.tm_min;
    QString now(QDateTime::currentDateTime().toString("dd.MM.yy hh:mm:ss"));
    bitstamp_dbg<0>.debug(str<>("candlestick_timer"), now.toStdString());
    update_ohlc_datasets();
  }
}
