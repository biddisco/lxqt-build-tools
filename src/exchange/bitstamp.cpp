#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
//
#include <QString>
//
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
//
#include <exec/inline_scheduler.hpp>
#include <exec/variant_sender.hpp>
#include <stdexec/execution.hpp>
//
#include <pika/execution/algorithms/just.hpp>
#include <pika/execution/algorithms/transfer_just.hpp>
#include <pika/execution_base/any_sender.hpp>
//
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"
#include "exchange/bitstamp.hpp"
#include "exchange/xrpl_network.hpp"
#include "network/evp-encrypt.hpp"
#include "network/qhttp-request-client.hpp"
#include "senders/qhttp-post-sender.hpp"
#include "senders/qtstdexec.hpp"
#include "util/datetime_utils.hpp"
#include "util/execute_os_command.hpp"
#include "util/json_qstring.hpp"
#include "util/stringutils.hpp"
#include "widgets/price_chart_widget.hpp"

// ----------------------------------------------------------------------------
using namespace grox;
using namespace grox::debug;
using namespace grox::senders;
using namespace nlohmann;
namespace ex = pika::execution::experimental;

// ----------------------------------------------------------------------------
template <int Level>
inline constexpr print_threshold<Level, 3> bitstamp_dbg("Bitstamp");

// ----------------------------------------------------------------------------
std::string what(std::exception_ptr const& eptr = std::current_exception())
{
  if (!eptr) { throw std::bad_exception(); }

  try
  {
    std::rethrow_exception(eptr);
  }
  catch (std::exception const& e)
  {
    return e.what();
  }
  catch (std::string const& e)
  {
    return e;
  }
  catch (char const* e)
  {
    return e;
  }
  catch (...)
  {
    return "who knows";
  }
}

// ----------------------------------------------------------------------------
bitstamp_network::bitstamp_network()
{
  exchange_name_ = "Bitstamp";
  for (auto n : {"Main", "Test", "Currency"})
  {
    bitstamp_account acct;
    acct.name_ = n;
    accounts_.push_back(acct);
  }
  using namespace std::literals;
  token_expiry_ = std::chrono::system_clock::now() - 60 * 1s;
}

// ----------------------------------------------------------------------------
bitstamp_network::~bitstamp_network() { bitstamp_dbg<0>.debug(ffmt<s20>("destructor")); }

// ----------------------------------------------------------------------------
void bitstamp_network::shut_down()
{
  bitstamp_dbg<0>.debug(ffmt<s20>("shutdown start"));
  exchange::shut_down();
}

// ----------------------------------------------------------------------------
bitstamp_account& bitstamp_network::get_account_by_name(std::string_view name)
{
  auto it = std::find_if(accounts_.begin(), accounts_.end(),
      [name](bitstamp_account& acct) { return name == acct.name_; });
  if (it == accounts_.end()) { throw std::runtime_error("account lookup failure"); }
  return (*it);
}

// ----------------------------------------------------------------------------
bitstamp_order_book const& bitstamp_network::get_orderbook(currency_pair const& cp) const
{
  ticker_data const tdata = get_subscribed_ticker_data(cp);
  return *dynamic_pointer_cast<bitstamp_order_book const>(tdata->orderbook_);
}

// ----------------------------------------------------------------------------
bool bitstamp_network::get_pass_authentication(bitstamp_account& account)
{
  account.API_user = execute_os_command("pass bitstamp/user");
  //
  std::string key = lowercase(fmt::format("pass bitstamp/account_key_{}", account.name_));
  account.API_key = execute_os_command(key.c_str());
  //
  std::string sec = lowercase(fmt::format("pass bitstamp/account_sec_{}", account.name_));
  account.API_secret = execute_os_command(sec.c_str());
  //
  if (account.API_user.empty() || account.API_key.empty() || account.API_secret.empty())
  {
    return false;
  }
  return true;
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
  auto web = stdexec::starts_on(QtStdExec::QThreadScheduler(), stdexec::just())    // Qt
      // | stdexec::let_value(
      //       std::bind(&bitstamp_network::request_websocket_token, this))    // Qt -> pika
      // | stdexec::then([this](QByteArray byteArray) {                        // pika
      //     std::string_view data(byteArray.constData(), byteArray.length());
      //     bitstamp_dbg<6>.debug(ffmt<s20>("Initialize"), "WebsocketToken", data);
      //     handle_websocket_token(data);
      //   })                                                      //
      // | stdexec::continues_on(QtStdExec::QThreadScheduler())    // pika -> Qt
      | stdexec::let_value(
            std::bind(&bitstamp_network::request_tickers_available, this))    // Qt -> pika
      | stdexec::then([this](QByteArray byteArray) {                          // pika
          std::string_view data(byteArray.constData(), byteArray.length());
          bitstamp_dbg<6>.debug(ffmt<s20>("Initialize"), "Tickers", data);
          handle_tickers_available(data);
        })                                                      //
      | stdexec::continues_on(QtStdExec::QThreadScheduler())    // pika -> Qt
      | stdexec::let_value(
            std::bind(&bitstamp_network::request_all_account_infos, this))    // Qt -> pika
      | stdexec::continues_on(QtStdExec::QThreadScheduler())                  // pika -> Qt
      | stdexec::let_value(
            std::bind(&bitstamp_network::request_all_account_orders, this))    // Qt -> pika
      | stdexec::then([this]() { emit network_initialized(this); }) |
      stdexec::upon_error([this](std::exception_ptr const& e) {
        std::lock_guard<std::mutex> l(candlestick_mutex_);
        bitstamp_dbg<0>.error(ffmt<s20>("Bitstamp initilize failed"), what(e));
      });

  // bitstamp_dbg<0>.debug(ffmt<s20>("SENDER"), grox::debug::print_type<decltype(snd0)>());
  stdexec::start_detached(std::move(web));
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_live_trades(currency_pair const& cp, bool enable)
{
  std::string ticker = currency_pair_lowercase_string(cp);
  json command;
  command["event"] = enable ? "bts:subscribe" : "bts:unsubscribe";
  command["data"]["channel"] = string_join("live_trades_", ticker);

  ticker_data tdata = get_subscribed_ticker_data(cp);
  bitstamp_dbg<4>.debug(ffmt<s20>("websocket trades"), command["event"],
      string_join("live_trades_", ticker), command.dump(4));

  if (enable)
  {
    using namespace std::placeholders;
    tdata->websockets_[network::streams::live_trades] = net::ws::qwebsocket_session::create(
        "bs::Trades " + ticker, bitstamp_websocket_address, bitstamp_websocket_port,
        command.dump(4), std::bind(bitstamp_network::new_live_trade_data_q, this, cp, _1));
  }
  else
  {
    tdata->websockets_[network::streams::live_trades].reset();
    tdata->websockets_.erase(network::streams::live_trades);
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

  ticker_data tdata = get_subscribed_ticker_data(cp);
  bitstamp_dbg<4>.debug(ffmt<s20>("websocket orders"), command["event"],
      string_join("order_book_", ticker), command.dump(4));

  if (enable)
  {
    using namespace std::placeholders;
    tdata->websockets_[network::streams::order_book] = net::ws::qwebsocket_session::create(
        "bs::Orders " + ticker, bitstamp_websocket_address, bitstamp_websocket_port,
        command.dump(4), std::bind(&bitstamp_network::new_orderbook_data_q, this, cp, _1));
  }
  else
  {
    tdata->websockets_[network::streams::order_book].reset();
    tdata->websockets_.erase(network::streams::order_book);
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

  ticker_data tdata = get_subscribed_ticker_data(cp);
  bitstamp_dbg<3>.debug(ffmt<s20>("websocket mytrades"), command["event"],
      string_join("private-my_trades_", ticker), command.dump(4));

  if (enable)
  {
    tdata->websockets_[network::streams::my_trades] =
        net::ws::qwebsocket_session::create("bs::MyTrades " + ticker, bitstamp_websocket_address,
            bitstamp_websocket_port, command.dump(4), [](QString const data) {
              //
              bitstamp_dbg<7>.debug(ffmt<s20>("(private) Trade data handler"), data.toStdString());
            });
  }
  else
  {
    tdata->websockets_[network::streams::my_trades].reset();
    tdata->websockets_.erase(network::streams::my_trades);
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

  ticker_data tdata = get_subscribed_ticker_data(cp);
  bitstamp_dbg<4>.debug(ffmt<s20>("websocket myorders"), command["event"],
      string_join("private-my_orders_", ticker), command.dump(4));

  if (enable)
  {
    tdata->websockets_[network::streams::my_orders] =
        net::ws::qwebsocket_session::create("bs::MyOrders " + ticker, bitstamp_websocket_address,
            bitstamp_websocket_port, command.dump(4), [this](QString const data) {
              std::string stdstring = data.toStdString();
              bitstamp_dbg<7>.debug(ffmt<s20>("Orders data"), stdstring);
              nlohmann::json jdata = nlohmann::json::parse(stdstring);
              if (jdata["event"] == "bts:subscription_succeeded")
              {
                bitstamp_dbg<0>.debug(ffmt<s20>("Orders data"), "bts:subscription_succeeded");
              }
              else
              {
                process_order(get_account_by_name("Main"), jdata["data"],
                    jdata["event"].get<std::string_view>());
              }
            });
  }
  else
  {
    // tdata->websocket_->write(command.dump(4));
    tdata->websockets_[network::streams::my_orders].reset();
    tdata->websockets_.erase(network::streams::my_orders);
  }
  return true;
}

// ----------------------------------------------------------------------------
// connect to a single stream
bool bitstamp_network::stream_subscribe(
    currency_pair const& cp, network::streams const stream, bool enabled, factory_function f)
{
  // always subscribe to a ticker before a stream it owns
  if (!ticker_subscribed(cp)) ticker_subscribe(cp);

  auto snd = stdexec::starts_on(QtStdExec::QThreadScheduler(), request_websocket_token())    //
      | stdexec::then([this](QByteArray byteArray) {                                         // pika
          std::string_view data(byteArray.constData(), byteArray.length());
          bitstamp_dbg<6>.debug(ffmt<s20>("Initialize"), "WebsocketToken", data);
          handle_websocket_token(data);
        })    //
      | stdexec::let_stopped([this]() {
          bitstamp_dbg<0>.debug(ffmt<s20>("Stopped"), "WebsocketToken already up-to-date");
          return stdexec::just();
        })    //
      |
      stdexec::then([this, cp, stream, enabled]() {    //
        bool ok = true;
        switch (stream)
        {
        case network::streams::my_trades: ok = subscribe_my_trades(cp, enabled); break;
        case network::streams::my_orders: ok = subscribe_my_orders(cp, enabled); break;
        case network::streams::live_trades: ok = subscribe_live_trades(cp, enabled); break;
        case network::streams::order_book: ok = subscribe_order_book(cp, enabled); break;
        case network::streams::price_data:
          std::cout << "Price data is a fake stream, please fix this for consistency" << std::endl;
          if (enabled)
          {
            // update candles regularly
            connect(timer_, SIGNAL(timeout()), this, SLOT(candlestick_timer_event()),
                Qt::QueuedConnection);
          }
          else { disconnect(timer_, SIGNAL(timeout()), 0, 0); }
          ok = true;
          break;
        default: ok = false; throw std::runtime_error("unknown stream");
        }
        if (ok) mark_stream_subscribed(cp, stream, enabled);
      })                                                        //
      | stdexec::continues_on(QtStdExec::QThreadScheduler())    //
      | stdexec::then([this, cp, stream, f]() {                 //
          f(cp, get_subscribed_ticker_data(cp), stream);
        });
  stdexec::start_detached(std::move(snd));
  // @todo : must return a sender here
  return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::can_send(currency_code const& c, exchange* dest)
{
  auto xrp_net = dynamic_cast<xrpl_network*>(dest);
  if (xrp_net && !xrp_net->testnet())
  {
    if (c.is_xrp() && (c.issuer_ == currencies::bitstamp_trust) &&
        ((c.code_ == "USD") || (c.code_ == "EUR")))
      return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
network::transaction_fees bitstamp_network::get_fees(currency_pair const& cp)
{
  if (transaction_fee_map_.contains(cp))
    return {transaction_fee_map_.at(cp), transaction_fee_map_.at(cp), 0, 0};
  currency_pair cp2 = reverse_pair(cp);
  if (transaction_fee_map_.contains(cp2))
    return {transaction_fee_map_.at(cp2), transaction_fee_map_.at(cp2), 0, 0};
  throw std::runtime_error("transaction fee lookup failed");
  return {0, 0, 0, 0};
}

// ----------------------------------------------------------------------------
bool bitstamp_network::make_payment(
    currency_amount const& c, basic_account* src, basic_account* dest)
{
  bitstamp_account* from = static_cast<bitstamp_account*>(src);
  ledger_wallet* to = static_cast<ledger_wallet*>(dest);
  bitstamp_dbg<0>.debug(ffmt<s20>("make_payment"), "amount", c.balance_, "currency", c, "from",
      from->name_, "to", to->public_,
      ((to->tag_ != 0) ? "(" + std::to_string(to->tag_) + ")" : ""));

  std::stringstream req_string;
  req_string << "amount=" << c.balance_ << "&address=" << to->public_;

  // issue a withdrawal payment to the wallet
  if (c.symbol_.is_xrp())
  {
    req_string << "&destination_tag"
               << "PUT SOMETHING IN HERE";
    //
    auto* client = signed_request(*from, "/api/v2/xrp_withdrawal/", req_string.str());
    auto web = stdexec::starts_on(exec::inline_scheduler(), stdexec::just(client))    // Qt
        | qhttp_post()                                                                // Qt -> pika
        | stdexec::then([this](QByteArray byteArray) {
            std::string_view data(byteArray.constData(), byteArray.length());
            bitstamp_dbg<0>.debug(ffmt<s20>("request CB"), "/api/v2/xrp_withdrawal/", data);
            emit transaction_event();
          });
  }
  // this is an IOU transfer
  else
  {
    req_string << "&currency= this is wrong" << c.symbol_.issuer_;
    //
    auto* client = signed_request(*from, "/api/v2/ripple_withdrawal/", req_string.str());
    auto web = stdexec::starts_on(exec::inline_scheduler(), stdexec::just(client))    // Qt
        | qhttp_post()                                                                // Qt -> pika
        | stdexec::then([this](QByteArray byteArray) {
            std::string_view data(byteArray.constData(), byteArray.length());
            bitstamp_dbg<0>.debug(ffmt<s20>("request CB"), "/api/v2/ripple_withdrawal/", data);
            emit transaction_event();
          });
  }

  return true;
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::request_account_info(bitstamp_account const& acct)
{
  auto* client = signed_request(acct, "/api/v2/balance/", "");
  return stdexec::just(client) | qhttp_post();
}

// ----------------------------------------------------------------------------
any_void_sender bitstamp_network::request_all_account_infos()
{
  // note pika::this_thread::sync_wait yields task, but stdexec::sync_wait blocks thread
  namespace tt = pika::this_thread::experimental;
  using namespace grox::debug;
  bitstamp_dbg<0>.debug(ffmt<s20>("all_account_infos"));

  // we can't block the Qt thread, so put async_scope onto a pika thread
  auto get_all_account_infos = [this]() {
    exec::async_scope scope;
    for (auto& acct : accounts())
    {
      auto handle_info = [this, &acct](QByteArray byteArray) {    // pika
        std::string_view data(byteArray.constData(), byteArray.length());
        bitstamp_dbg<6>.debug(ffmt<s20>("Initialize"), "AccountInfo", data);
        handle_account_info(acct, data);
      };

      auto snd = ex::starts_on(QtStdExec::QThreadScheduler(), ex::just())            // Qt
          | ex::let_value([this, &acct]() { return request_account_info(acct); })    // -> pika
          | ex::then(handle_info);

      scope.spawn(std::move(snd));
    }

    bitstamp_dbg<2>.debug(ffmt<s20>("SYNC_WAIT"), "scope", "get_all_account_infos");
    tt::sync_wait(scope.on_empty());
    bitstamp_dbg<2>.debug(ffmt<s20>("COMPLETE"), "scope", "get_all_account_infos");
  };

  // must be on a pika thread if we are using sync_wait
  auto snd = stdexec::just()                               //
      | stdexec::continues_on(default_pool_scheduler())    //
      | stdexec::then(get_all_account_infos);

  return any_void_sender{std::move(snd)};
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::request_websocket_token()
{
  if (token_valid(token_expiry_))
  {
    bitstamp_dbg<2>.debug(ffmt<s20>("websocket_token"), "Still valid");
    return any_bytearray_sender{stdexec::just_stopped()};
  }
  //
  bitstamp_dbg<2>.debug(ffmt<s20>("websocket_token"), "Fetching new");
  auto* client = signed_request(get_account_by_name("Main"), "/api/v2/websockets_token/", "");
  return stdexec::just(client) | qhttp_post();
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::request_account_orders(bitstamp_account const& acct)
{
  auto* client = signed_request(acct, "/api/v2/open_orders/all/", "");
  return stdexec::just(client) | qhttp_post();
}

// ----------------------------------------------------------------------------
any_void_sender bitstamp_network::request_all_account_orders()
{
  // note pika::this_thread::sync_wait yields task, but stdexec::sync_wait blocks thread
  namespace tt = pika::this_thread::experimental;
  using namespace grox::debug;
  bitstamp_dbg<0>.debug(ffmt<s20>("all_account_orders"));

  // we can't block the Qt thread, so put async_scope onto a pika thread
  auto get_all_orders = [this]() {
    exec::async_scope scope;
    for (auto& acct : accounts())
    {
      auto handle_info = [this, &acct](QByteArray byteArray) {    // pika
        std::string_view data(byteArray.constData(), byteArray.length());
        bitstamp_dbg<6>.debug(ffmt<s20>("Initialize"), "AccountOrders", data);
        handle_open_orders(acct, data);
      };

      auto snd = ex::starts_on(QtStdExec::QThreadScheduler(), ex::just())              // Qt
          | ex::let_value([this, &acct]() { return request_account_orders(acct); })    // -> pika
          | ex::then(handle_info);

      scope.spawn(std::move(snd));
    }

    bitstamp_dbg<2>.debug(ffmt<s20>("SYNC_WAIT"), "scope", "request_all_account_orders");
    tt::sync_wait(scope.on_empty());
    bitstamp_dbg<2>.debug(ffmt<s20>("COMPLETE"), "scope", "request_all_account_orders");
  };

  // must be on a pika thread if we are using sync_wait
  auto snd = stdexec::just()                               //
      | stdexec::continues_on(default_pool_scheduler())    //
      | stdexec::then(get_all_orders);

  return any_void_sender{std::move(snd)};
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::request_tickers_available()
{
  std::string url = fmt::format(
      "https://{}:{}{}", bitstamp_https_address, bitstamp_https_port, "/api/v2/ticker/");
  auto* client = net::http::qhttp_request_client::create(*global_settings.networkmanager_, url);
  return stdexec::just(client) | qhttp_post(http_request_type::http_get);
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::request_cancel_order(trade_data const& t)
{
  std::string query = fmt::format("?&id={}", t.id_);
  std::string req = fmt::format("/api/v2/cancel_order/");

  bitstamp_account& acct = get_account_by_name(t.wallet_);
  auto* client = signed_request(acct, req, query);
  return stdexec::just(client) | qhttp_post();
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::request_limit_order(
    bitstamp_account const& acct, trade_data const& t)
{
  double amount = t.get_xrp_amount();

  // bitstamp trade pair is always xrpusd, so swap symbols accordingly
  bitstamp_dbg<0>.error(ffmt<s20>("limit-order"), "@TODO USD assumption false");
  std::string req, query;

  if (t.get_trade_type() == trade_type::buy)
  {
    std::string ticker = lowercase(t.taker_payc_.code_ + t.taker_getc_.code_);
    req = fmt::format("/api/v2/buy/{}/", ticker);
    query = fmt::format("?&amount={}&price={}", currency_precision(amount, t.taker_payc_),
        to_string_with_precision(t.exchange_rate_, 5));
  }
  else
  {
    std::string ticker = lowercase(t.taker_getc_.code_ + t.taker_payc_.code_);
    req = fmt::format("/api/v2/sell/{}/", ticker);
    query = fmt::format("?&amount={}&price={}", currency_precision(amount, t.taker_payc_),
        to_string_with_precision(t.exchange_rate_, 5));
  }
  //
  bitstamp_dbg<0>.debug(ffmt<s20>("limit-order"),
      (t.get_trade_type() == trade_type::buy ? "Buy" : "Sell"), req, query);

  auto* client = signed_request(acct, req, query);
  return stdexec::just(client) | qhttp_post();
}

// ----------------------------------------------------------------------------
currency_code add_fiat_issuer(currency_code const& c)
{
  static std::vector<std::string> fiat{"USD", "EUR", "GBP"};
  for (auto const& f : fiat)
  {
    if ((c.code_ == f)) return {currencies::bitstamp_trust, c.code_};
  }
  return c;
}

// ----------------------------------------------------------------------------
currency_pair bitstamp_network::split_token_string(std::string utoken) const
{
  for (auto const& cp : get_currency_pairs())
  {
    if (currency_pair_string(cp, "", false) == utoken) { return cp; }
  }
  bitstamp_dbg<0>.error(ffmt<s20>("split_token_string"), "Currency pair not found", utoken);
  return currency_pair();
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_account_info(bitstamp_account& acct, std::string_view data)
{
  try
  {
    nlohmann::json jdata = nlohmann::json::parse(data);
    bitstamp_dbg<6>.debug(ffmt<s20>("account info"), jdata.dump(4));
    bitstamp_dbg<2>.debug(ffmt<s20>("account info"), "Processing", acct.name_);

    std::regex bal_regex("_balance", std::regex_constants::icase);
    std::regex tok_regex("([^_]+)_.*");
    std::regex trans_fee_regex("[^_]+_fee", std::regex_constants::icase);
    std::regex withd_fee_regex("[^_]+_withdrawal_fee", std::regex_constants::icase);
    std::smatch mtch;
    for (auto const& [key, val] : jdata.items())
    {
      // for any currency with a non zero balance, add it to our account
      double value = std::stod(JCHARP(val));
      if (std::regex_search(key, bal_regex) && (value > 0))
      {
        if (std::regex_match(key, mtch, tok_regex))
        {
          std::string ltoken = mtch[1];
          std::string utoken = uppercase(ltoken);
          currency_amount cur{{"", utoken},                       //
              value,                                              //
              std::stod(JCHARP(jdata[ltoken + "_available"])),    //
              std::stod(JCHARP(jdata[ltoken + "_reserved"])),     //
              // std::stod(JCHARP(jdata[ltoken + "__withdrawal_fee"])),    //
              nullptr};
          acct.add_currency(cur);

          bitstamp_dbg<5>.debug(ffmt<s20>("account info"), cur);
        }
      }

      // add fees (withdrawal/transaction)
      if (std::regex_search(key, withd_fee_regex))
      {
        if (std::regex_match(key, mtch, tok_regex))
        {
          // @todo - this needs to be checked to correctly handle non 3 letter codes
          std::string utoken = uppercase(mtch[1]);
          withdrawal_fee_map_[{"", utoken}] = value;
          bitstamp_dbg<5>.debug(ffmt<s20>("account info"), "withdrawal fee", utoken, value);
        }
      }
      else if (std::regex_search(key, trans_fee_regex))
      {
        if (std::regex_match(key, mtch, tok_regex))
        {
          // bitstamp (so far) always quotes fees as token_fiat not fiat_token
          std::string utoken = uppercase(mtch[1]);
          currency_pair cp = split_token_string(utoken);
          transaction_fee_map_[cp] = value;
          bitstamp_dbg<5>.debug(
              ffmt<s20>("account info"), "transaction fee", cp.c1_, cp.c2_, value);
        }
      }
    }

    emit update_wallet_widget(&acct);
  }
  catch (std::exception_ptr const& e)
  {
    bitstamp_dbg<0>.error(ffmt<s20>("Account info failed"));
    std::rethrow_exception(e);
  }
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_websocket_token(std::string_view data)
{
  try
  {
    nlohmann::json jdata = nlohmann::json::parse(data);
    bitstamp_dbg<5>.debug(ffmt<s20>("websocket token"), jdata.dump());
    //
    using namespace std::literals;
    auto valid_sec = jdata["valid_sec"].get<int>();
    websocket_token_ = jdata["token"].get<std::string>();
    websocket_user_id_ = std::to_string(jdata["user_id"].get<int>());
    token_expiry_ = std::chrono::system_clock::now() + valid_sec * 1s;

    bitstamp_dbg<2>.debug(ffmt<s20>("websocket_token"),
        token_valid(token_expiry_) ? "valid until" : "expired",
        fmt::format("{:%Y-%m-%d %X}", round<std::chrono::seconds>(token_expiry_.load())));
  }
  catch (std::exception_ptr const& e)
  {
    bitstamp_dbg<0>.error(ffmt<s20>("websocket token"), "Failed to renew");
    std::rethrow_exception(e);
  }
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_open_orders(bitstamp_account& acct, std::string_view data)
{
  nlohmann::json jdata = nlohmann::json::parse(data);
  bitstamp_dbg<6>.debug(ffmt<s20>("open_orders"), jdata.dump());
  for (auto const& [key, val] : jdata.items())
  {
    std::string const jstring = val[std::string_view("currency_pair")];
    auto const& [c1, c2] = split_currency_pair_string(jstring, '/');

    double amount = std::stod(JCHARP(val["amount"]));
    double price = std::stod(JCHARP(val["price"]));
    double fee_percent = 0;
    double fee_fixed = 0;
    // 0=buy, 1=sell
    auto trade_type_ = (val["type"] == "0") ? trade_type::buy : trade_type::sell;

    if (trade_type_ == trade_type::sell)
    {
      trade_data t{this->get_instance(), acct.name_, {"", c2.cbegin()}, {"", c1.cbegin()},
          amount * price, amount, price, fee_percent, fee_fixed,
          std::stoull(val["id"].get<std::string>()), val["datetime"], true};
      acct.add_trade(std::move(t), true);
    }
    else
    {
      trade_data t{this->get_instance(), acct.name_, {"", c1.cbegin()}, {"", c2.cbegin()}, amount,
          amount * price, price, fee_percent, fee_fixed, std::stoull(val["id"].get<std::string>()),
          val["datetime"], true};
      acct.add_trade(std::move(t), true);
    }
  }
  emit update_wallet_widget(&acct);
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_tickers_available(std::string_view data)
{
  try
  {
    nlohmann::json jdata = nlohmann::json::parse(data);
    for (auto const& [key, val] : jdata.items())
    {
      json::string_t jstring = val[std::string_view("pair")];
      auto const& [c1, c2] = string_to_pair(jstring, "/");
      bitstamp_dbg<6>.debug(ffmt<s20>("Currency pair"), jstring, c1, c2);
      add_currency_pair({c1, c2});
    }
  }
  catch (std::exception_ptr const& e)
  {
    bitstamp_dbg<0>.error(ffmt<s20>("Ticker data error"));
    std::rethrow_exception(e);
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
void bitstamp_network::process_order(bitstamp_account& acct, json& jdata, std::string_view event)
{
  auto& trades = acct.offers_;
  //
  std::uint64_t id = jdata["id"];
  auto find_by_id = [id](trade_data& t) { return t.id_ == id; };

  if (event == "order_deleted")
  {
    auto trade = std::find_if(trades.begin(), trades.end(), find_by_id);
    if (trade == trades.end())
    {
      bitstamp_dbg<0>.error(ffmt<s20>("Order not found"), ffmt<dec18>(id));
    }
    else
    {
      bitstamp_dbg<0>.debug(ffmt<s20>("Order deleted"), ffmt<dec18>(id));
      trades.erase(trade);
    }
  }
  if (event == "order_created")
  {
    auto trade = std::find_if(trades.begin(), trades.end(), find_by_id);
    if (trade == trades.end())
    {
      bitstamp_dbg<0>.error(ffmt<s20>("Order not found"), ffmt<dec18>(id));
    }
    else
    {
      if (trade->confirmed_ == false)
      {
        bitstamp_dbg<0>.debug(ffmt<s20>("Order created"), ffmt<dec18>(id), "confirmed");
        trade->confirmed_ = true;
      }
      else { bitstamp_dbg<0>.error(ffmt<s20>("Order created"), ffmt<dec18>(id), "already active"); }
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
    bitstamp_account const& acct, std::string const& url_path, std::string const& url_query)
{
  std::string api_key = acct.API_key;
  std::string api_secret = acct.API_secret;
  secure_string randbytes = generate_random_alphanumeric_string(encryption::KEY_SIZE, 81192);
  encryption encryptor(api_key, randbytes);
  //
  using namespace std::chrono;
  milliseconds timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

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
  bitstamp_dbg<0>.debug(ffmt<s20>("account_request"), urlstring, string_to_sign);

  QNetworkRequest request(QUrl(to_qstring(urlstring)));
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
    bitstamp_network* exchange, currency_pair const cp, QString const data)
{
  bitstamp_dbg<5>.debug(ffmt<s20>("Orderbook"), "Ticker", currency_pair_string(cp));
  bitstamp_dbg<7>.debug(ffmt<s20>("Orderbook data"), data.toStdString());

  // if shutdown was started after this data was sent by the remote source
  // then it can be ignored/dropped as we will not handle it anyway
  std::lock_guard l(exchange->async_mutex_);
  if (exchange->closing_down_)
  {
    bitstamp_dbg<0>.error(ffmt<s20>("Orderbook data"), "Shutdown in progress: ignoring data");
    return;
  }

  auto process = [exchange, cp, data]() {
    ticker_data const tdata = exchange->get_subscribed_ticker_data(cp);
    try
    {
      dynamic_pointer_cast<bitstamp_order_book>(tdata->orderbook_)->accept_json_bitstamp(data);
    }
    catch (...)
    {
      bitstamp_dbg<0>.error(ffmt<s20>("Orderbook error"), currency_pair_string(cp),
          tdata->orderbook_, data.toStdString());
    }
    //
    tdata->orderbook_subscribers_.publish(cp);
  };

  stdexec::sender auto snd =
      stdexec::starts_on(default_pool_scheduler(), stdexec::just()) | stdexec::then(process);
  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
void bitstamp_network::new_live_trade_data_q(
    bitstamp_network* exchange, currency_pair cp, QString const data)
{
  bitstamp_dbg<4>.debug(ffmt<s20>("Live Trade"), "Ticker", currency_pair_string(cp));
  bitstamp_dbg<5>.debug(ffmt<s20>("Trade data"), data.toStdString());

  // if shutdown was started after this data was sent by the remote source
  // then it can be ignored/dropped as we will not handle it anyway
  std::lock_guard l(exchange->async_mutex_);
  if (exchange->closing_down_)
  {
    bitstamp_dbg<0>.error(ffmt<s20>("trade data"), "Shutdown in progress: ignoring data");
    return;
  }
  if (!startswith(data, "{\"data\":")) return;

  auto process = [exchange, data, cp]() {
    std::string stdstring = data.toStdString();
    nlohmann::json jdata = nlohmann::json::parse(stdstring)["data"];
    bitstamp_dbg<7>.debug(ffmt<s20>("Trade data parsed"), jdata.dump(4));
    live_trade_data trade_data = jdata.get<live_trade_data>();
    //
    exchange->get_subscribed_ticker_data(cp)->live_trade_subscribers_.publish(cp, trade_data);
  };

  stdexec::sender auto snd =
      stdexec::starts_on(default_pool_scheduler(), stdexec::just()) | stdexec::then(process);
  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
// OHLC candlestick updates
// ----------------------------------------------------------------------------
// typically called once per minute by the application to update data regularly
void bitstamp_network::update_ohlc_datasets()
{
  for (auto& [cp, data] : tickers_subscribed_)
  {
    if (!candlestick_updates_active_.contains(cp)) { update_ohlc_data(cp, data); }
    else
    {
      bitstamp_dbg<0>.warning(ffmt<s20>("ohlc active"), currency_pair_lowercase_string(cp));
      throw std::runtime_error("candlestick_updates_active_ is it really needed?");
    }
  }
}

// ----------------------------------------------------------------------------
std::uint64_t bitstamp_network::handle_price_history(std::string_view data)
{
  nlohmann::json jdata = nlohmann::json::parse(data);
  bitstamp_dbg<9>.debug(ffmt<s20>("price history"), jdata.dump(4));
  //
  auto subsect = jdata["data"]["prices"]["all"]["prices"];
  bitstamp_dbg<7>.debug(ffmt<s20>("price history"), subsect.dump(4));
  auto prices = subsect.get<std::vector<price>>();
  auto first_time = prices[prices.size() - 2].time;
  bitstamp_dbg<1>.debug(
      ffmt<s20>("First date"), first_time, secs_unix_to_calendar_time_local(first_time));
  return first_time;
}

// ----------------------------------------------------------------------------
void bitstamp_network::update_ohlc_data(currency_pair cp, ticker_data tdata)
{
  // does not matter if ticker already in map, it will be cleaned out when done
  {
    std::lock_guard<std::mutex> l(candlestick_mutex_);
    candlestick_updates_active_.insert(cp);
  }

  // find the most recent sample we currently have
  std::uint64_t start_t_sec =
      static_cast<uint64_t>(tdata->view_->get_last_sample_time_msec(false)) / 1000;

  // start time is last data sample + 60s
  any_uint64_sender snd{stdexec::just(start_t_sec + 60)};
  if (start_t_sec == 0)
  {
    bitstamp_dbg<0>.debug(
        ffmt<s20>("No existing data"), tdata->view_->get_ticker_string(), "finding start");
    snd = any_uint64_sender{request_price_history(cp)    //
        | stdexec::then([this, cp, tdata](QByteArray byteArray) {
            std::string_view data(byteArray.constData(), byteArray.length());
            bitstamp_dbg<9>.debug(
                ffmt<s20>("price history"), tdata->view_->get_ticker_string(), data);
            return handle_price_history(data);
          })    //
        | stdexec::continues_on(QtStdExec::QThreadScheduler())};
  }

  auto snd2 = stdexec::starts_on(QtStdExec::QThreadScheduler(), std::move(snd))    //
      | stdexec::let_value([this, cp, tdata](std::uint64_t start_t_sec) {
          // we do not want a candle for the current minute, round to prev 60s
          std::uint64_t unixtime_secs = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
          unixtime_secs = unixtime_secs - (unixtime_secs % 60);
          if ((unixtime_secs - start_t_sec) < 60)
          {
            bitstamp_dbg<0>.debug(ffmt<s20>("candlesticks"), tdata->view_->get_ticker_string(),
                "up to date", secs_unix_to_calendar_time_local(start_t_sec));
            bitstamp_dbg<0>.debug(ffmt<s20>("OHLC up-to-date"));
            {
              std::lock_guard<std::mutex> l(candlestick_mutex_);
              candlestick_updates_active_.erase(cp);
            }
            return any_bytearray_sender{stdexec::just_stopped()};
          }

          std::uint64_t samples = (unixtime_secs - start_t_sec) / 60;
          bitstamp_dbg<0>.debug(ffmt<s20>("requesting"), tdata->view_->get_ticker_string(), "from",
              secs_unix_to_calendar_time_local(start_t_sec), samples);
          return request_new_ohlc_data(cp, start_t_sec, samples);
        })    //
      | stdexec::then([this, cp, tdata](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          bitstamp_dbg<4>.debug(ffmt<s20>("OHLC (lambda)"), tdata->view_->get_ticker_string());
          handle_new_ohlc_data(tdata, data);
          update_ohlc_data(cp, tdata);
        })    //
      | stdexec::upon_error([this, cp](std::exception_ptr const& e) {
          std::lock_guard<std::mutex> l(candlestick_mutex_);
          {
            std::lock_guard<std::mutex> l(candlestick_mutex_);
            candlestick_updates_active_.erase(cp);
          }
          bitstamp_dbg<0>.error(ffmt<s20>("OHLC"), what(e));
        });

  stdexec::start_detached(std::move(snd2));
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
  if (start_t == 0) { req = fmt::format("/api/v2/ohlc/{}/?step=60&limit=1000", ticker_lowercase); }
  else
  {
    if (samples >= 1000)
    {
      bitstamp_dbg<5>.debug(ffmt<s20>("Limiting request"), samples);
      samples = 1000;
    }
    std::string start = std::to_string(start_t);
    std::string limit = std::to_string(samples);
    // send a request for ticker data using the io context thread to make the request
    req = fmt::format("/api/v2/ohlc/{}/?step=60&start={}&limit={}", ticker_lowercase, start, limit);
    bitstamp_dbg<0>.debug(ffmt<s20>("request"), ticker_lowercase, req);
  }

  // @todo : add error hander
  std::string url =
      fmt::format("https://{}:{}{}", bitstamp_https_address, bitstamp_https_port, req);
  net::http::client_ptr client =
      net::http::qhttp_request_client::create(*global_settings.networkmanager_, url);
  return {stdexec::just(client) | qhttp_post(http_request_type::http_get)};
}

// ----------------------------------------------------------------------------
any_bytearray_sender bitstamp_network::request_price_history(currency_pair cp)
{
  std::string ticker_lowercase = currency_pair_lowercase_string(cp);
  std::string url = fmt::format("https://{}:{}/api-internal/price-history/{}/", "www.bitstamp.net",
      bitstamp_https_port, ticker_lowercase);
  bitstamp_dbg<0>.debug(ffmt<s20>("request"), ticker_lowercase, url);
  auto* client = net::http::qhttp_request_client::create(*global_settings.networkmanager_, url);
  return any_bytearray_sender{stdexec::just(client) | qhttp_post(http_request_type::http_get)};
}

// ----------------------------------------------------------------------------
void bitstamp_network::place_buy_sell_orders(
    basic_account* acct, std::vector<trade_data> const& trades)
{
  bitstamp_account* bacct = static_cast<bitstamp_account*>(acct);
  // std::atomic<int> counter{0};
  std::vector<ex::unique_any_sender<>> trade_orders;
  for (auto const& trade : trades)
  {
    auto snd = request_limit_order(*bacct, trade)    //
        | stdexec::then([&, trade = trade, this](QByteArray byteArray) mutable {
            std::string_view data(byteArray.constData(), byteArray.length());
            json jdata = json::parse(data);
            bitstamp_dbg<1>.debug(ffmt<s20>("buy_sell response"), jdata.dump(4));
            if (jdata.contains("error"))
              bitstamp_dbg<0>.error(ffmt<s20>("buy_sell error"), jdata.dump(4));
            else
            {
              // counter++;
              trade.id_ = std::stoll(JCHARP(jdata["id"]));
              trade.confirmed_ = true;
              trade.datetime_ = JCHARP(jdata["datetime"]);
              double amount = std::stod(JCHARP(jdata["amount"]));
              if (trade.get_price() != std::stod(JCHARP(jdata["price"])))
              {
                bitstamp_dbg<0>.error(ffmt<s20>("buy_sell price"), trade.get_price(),
                    std::stod(JCHARP(jdata["price"])));
              }
              bacct->add_trade(std::move(trade), true);
              emit update_wallet_widget(bacct);
            }
          });
    trade_orders.push_back(std::move(snd));
  }

  auto all_done = ex::when_all_vector(std::move(trade_orders)) | ex::let_value([this, bacct]() {
    auto snd = request_account_orders(*bacct)                    //
        | stdexec::then([this, bacct](QByteArray byteArray) {    // pika
            std::string_view data(byteArray.constData(), byteArray.length());
            bitstamp_dbg<5>.debug(ffmt<s20>("Initialize"), "OpenOrders", data);
            handle_open_orders(*bacct, data);
          });
    return snd;
  });

  ex::start_detached(std::move(all_done));
}

// ----------------------------------------------------------------------------
stream_set bitstamp_network::ticker_subscribe(currency_pair const& cp)
{
  // exit if this exchange has already subscribed to this ticker
  std::string cps = currency_pair_string(cp);
  if (ticker_subscribed(cp))
  {
    bitstamp_dbg<2>.debug(ffmt<s20>("subscription"), cps, "already subscribed");
    return stream_set{};
  }
  bitstamp_dbg<0>.debug(ffmt<s20>("subscribing"), cps);

  // create a new data view from hdf5
  std::shared_ptr<ohlc_dataset_view> view = std::make_shared<ohlc_dataset_view>("bitstamp", cp);
  std::shared_ptr<bitstamp_order_book> orderbook = std::make_shared<bitstamp_order_book>();
  // add the subscribed ticker/data/plot to our list for tracking
  ticker_data data =
      std::make_shared<ticker_subscription>(shared_from_this(), view, orderbook, nullptr);
  tickers_subscribed_.insert({cp, data});
  return websocket_streams();
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_new_ohlc_data(ticker_data tdata, std::string_view data)
{
  json jdata;
  try
  {
    // convert json data into vectors of actual data
    jdata = json::parse(data)["data"]["ohlc"];
  }
  catch (std::exception& e)
  {
    bitstamp_dbg<0>.error(
        ffmt<s20>("JSON error"), "parsing OHLC data:", e.what(), "\n", data, "\n\n");
    return;
  }
  try
  {
    bitstamp_dbg<0>.debug(ffmt<s20>("OHLC received"), tdata->view_->get_ticker_string(),
        ffmt<dec4>(jdata.size()), "json OHLC samples");
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
    bitstamp_dbg<5>.debug(ffmt<s20>("Converted"), tdata->view_->get_ticker_string(),
        new_ohlc_samples.size(), "new OHLC samples");
    tdata->view_->merge_data(ohlc_data_resolutions::minute, new_ohlc_samples);
    // what is the last sample we currently have
    auto last_time = tdata->view_->get_last_sample_time_msec(false);
    bitstamp_dbg<0>.debug(ffmt<s20>("data merged up to"), tdata->view_->get_ticker_string(),
        msecs_unix_to_calendar_time_local(last_time));
    tdata->view_->delete_live_data_up_to(last_time);
    // replot on a Qt thread
    QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [=]() {
      if (tdata->chart_widget_) tdata->chart_widget_->replot();
    });
  }
  catch (std::exception& e)
  {
    bitstamp_dbg<0>.error(
        ffmt<s20>("JSON error"), "processing OHLC data:", e.what(), "\n", data, "\n\n");
    std::terminate();
  }
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
    bitstamp_dbg<0>.debug(ffmt<s20>("candlestick_timer"), now.toStdString());
    update_ohlc_datasets();
  }
}
