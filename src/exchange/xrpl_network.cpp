// STL
#include <string>
// Qt
#include <QDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QObject>
#include <QPlainTextEdit>
#include <QString>
// extern
#include <fmt/format.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/Sign.h>
#include <xrpl/protocol/UintTypes.h>
//
//
#include <exec/async_scope.hpp>
#include <exec/inline_scheduler.hpp>
#include <exec/variant_sender.hpp>
#include <stdexec/execution.hpp>
//
#include <pika/execution/algorithms/just.hpp>
#include <pika/execution/algorithms/transfer_just.hpp>
//
#include "debug/print.hpp"
#include "exchange/bitstamp.hpp"
#include "exchange/order_book.hpp"
#include "exchange/xrpl.hpp"
#include "exchange/xrpl_network.hpp"
#include "network/evp-encrypt.hpp"
#include "senders/qhttp-post-sender.hpp"
#include "senders/qtstdexec.hpp"
#include "util/stringutils.hpp"
#include "widgets/xrp_functions.hpp"

// ----------------------------------------------------------------------------
using namespace grox;
using namespace grox::debug;
using namespace grox::senders;
using namespace nlohmann;
//
template <int Level>
inline constexpr print_threshold<Level, 7> xrpnet_dbg("XRP-legr");

// ----------------------------------------------------------------------------
xrpl_network::xrpl_network(bool testnet)
  : testnet_(testnet)
{
  exchange_name_ = testnet_ ? "XRPL Testnet" : "XRPL Mainnet";
}

// ----------------------------------------------------------------------------
xrpl_network::~xrpl_network() { xrpnet_dbg<0>.debug(str<>("destructor"), "testnet ", testnet()); }

// ----------------------------------------------------------------------------
void xrpl_network::initialize()
{
  if (!testnet())
  {
    add_currency_pair({{currency::bitstamp_trust, "USD"}, currency_code{"", "XRP"}});
    add_currency_pair({{currency::bitstamp_trust, "EUR"}, currency_code{"", "XRP"}});
    add_currency_pair({{currency::gatehub_trust, "USD"}, currency_code{"", "XRP"}});
    add_currency_pair({{currency::gatehub_trust, "EUR"}, currency_code{"", "XRP"}});
  }

  // spawn a task that performs init functions, we must do this on a pika thread because
  // we can't sync_wait on the QApplication main thread
  auto wait_for_init = [this]() {
    exec::async_scope scope;
    //
    get_all_account_infos(scope);
    get_all_account_lines(scope);
    get_all_account_offers(scope);
    //
    stdexec::sync_wait(scope.on_empty());
    xrpnet_dbg<5>.debug(str<>("initialize scope"), "complete");
    emit network_initialized(this);
  };

  stdexec::sender auto snd =
      stdexec::on(default_pool_scheduler(), stdexec::just()) | stdexec::then(wait_for_init);
  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
bool xrpl_network::testnet() const { return testnet_; }

// ----------------------------------------------------------------------------
std::string xrpl_network::websocket_address() const
{
  if (testnet_) return testnet_websocket_address;
  return ripple_websocket_address;
}
int xrpl_network::websocket_port() const
{
  if (testnet_) return testnet_websocket_port;
  return ripple_websocket_port;
}

std::string xrpl_network::jsonrpc_address() const
{
  if (testnet_) return testnet_json_rpc_address;
  return ripple_jsonrpc_address;
}
int xrpl_network::jsonrpc_port() const
{
  if (testnet_) return testnet_json_rpc_port;
  return ripple_jsonrpc_port;
}

// ----------------------------------------------------------------------------
bool xrpl_network::can_send(currency const& c, exchange* dest)
{
  // yes to anything if the source is also an xrpl wallet
  if (dynamic_cast<xrpl_network*>(dest))
  {
    return dynamic_cast<xrpl_network*>(dest)->testnet() == testnet();
  }
  else if (!testnet() && dynamic_cast<bitstamp_network*>(dest))
  {
    if (c.is_xrp() ||
        ((c.issuer_ == currency::bitstamp_trust) && ((c.code_ == "USD") || (c.code_ == "EUR"))))
    {
      return true;
    }
  }
  return false;
}

// ----------------------------------------------------------------------------
xrpl_order_book const& xrpl_network::get_orderbook(currency_pair const& cp) const
{
  const ticker_data tdata = get_subscribed_ticker_data(cp);
  return *dynamic_pointer_cast<xrpl_order_book const>(tdata->orderbook_);
}

// ----------------------------------------------------------------------------
// connect to a single stream
bool xrpl_network::stream_subscribe(
    currency_pair const& cp, network::streams const stream, bool enabled, factory_function f)
{
  // always subscribe to a ticker before a stream it owns
  if (!ticker_subscribed(cp)) ticker_subscribe(cp);

  auto snd = stdexec::on(QtStdExec::QThreadScheduler(), stdexec::just())    //
      | stdexec::then([this, cp, stream, enabled]() {                       //
          bool ok = true;
          switch (stream)
          {
          case network::streams::account_changes: ok = subscribe_accounts(); break;
          case network::streams::order_book: ok = subscribe_order_book(cp, enabled); break;
          default: ok = false; throw std::runtime_error("unknown stream");
          }
          if (ok) mark_stream_subscribed(cp, stream, enabled);
        })                                         //
      | stdexec::then([this, cp, stream, f]() {    //
          f(cp, get_subscribed_ticker_data(cp), stream);
        });
  stdexec::start_detached(std::move(snd));
  // @todo : must return a sender here
  return true;
}

// ----------------------------------------------------------------------------
stream_set xrpl_network::ticker_subscribe(currency_pair const& cp)
{
  // exit if this exchange has already subscribed to this ticker
  std::string cps = currency_pair_string(cp);
  if (ticker_subscribed(cp))
  {
    xrpnet_dbg<0>.debug(str<>("subscription"), cps, "subscribed");
    return stream_set{};
  }
  xrpnet_dbg<0>.debug(str<>("subscribing"), cps);

  // create a new data view from hdf5
  // std::shared_ptr<ohlc_dataset_view> view = std::make_shared<ohlc_dataset_view>("xrpl", c1, c2);
  std::shared_ptr<xrpl_order_book> orderbook = std::make_shared<xrpl_order_book>();
  // add the subscribed ticker/data/plot to our list for tracking
  ticker_data data =
      std::make_shared<ticker_subscription>(shared_from_this(), nullptr, orderbook, nullptr);
  tickers_subscribed_.insert({cp, data});
  return websocket_streams();
}
//// ----------------------------------------------------------------------------
//bool xrpl_network::websocket_connect(net::contexts& io_contexts, stream_set const& streams)
//{
//  bool ok = true;
//  for (auto const& s : streams)
//  {
//    if (s == network::streams::order_book)
//      ok &= subscribe_order_book(io_contexts);
//  }
//  return ok;
//}

//// ----------------------------------------------------------------------------
//bool xrpl_network::websocket_disconnect(
//  net::contexts& /*io_contexts*/, stream_set const& streams)
//{
//  bool ok = true;
//  for (auto const& s : streams)
//  {
//    if (s == network::streams::order_book)
//      ws_orderbook->shutdown_blocking();
//    if (s == network::streams::accounts)
//      ws_accounts->shutdown_blocking();
//  }
//  return ok;
//}

// ----------------------------------------------------------------------------
void xrpl_network::shut_down()
{
  // do not allow shutdown / async operations concurrently
  closing_down_ = true;
  std::lock_guard l(async_mutex_);
  //
  if (ws_orderbook) { ws_orderbook.reset(); }
  if (ws_accounts) { ws_accounts.reset(); }
}

// ----------------------------------------------------------------------------
void xrpl_network::add_wallet(ledger_wallet const& w) { subscribed_wallets_.push_back(w); }

// ----------------------------------------------------------------------------
bool xrpl_network::subscribe_order_book(currency_pair const& cp, bool enable)
{
  using namespace std::placeholders;
  // flip currency pair around if xrp is second
  currency_pair cp2 = cp;
  if (!std::get<0>(cp).is_xrp()) { cp2 = {std::get<1>(cp), std::get<0>(cp)}; }
  //startswith
  json command;
  command["command"] = "subscribe";
  // buying xrp
  json buy_xrp;
  buy_xrp["taker_gets"]["currency"] = std::get<0>(cp2).code_;
  buy_xrp["taker_pays"]["currency"] = std::get<1>(cp2).code_;
  buy_xrp["taker_pays"]["issuer"] = std::get<1>(cp2).issuer_;
  buy_xrp["snapshot"] = true;
  // selling xrp
  json sell_xrp;
  sell_xrp["taker_pays"]["currency"] = std::get<0>(cp2).code_;
  sell_xrp["taker_gets"]["currency"] = std::get<1>(cp2).code_;
  sell_xrp["taker_gets"]["issuer"] = std::get<1>(cp2).issuer_;
  sell_xrp["snapshot"] = true;
  // subscribe to 2 books
  command["books"] = json::array({buy_xrp, sell_xrp});
  std::string subscription = command.dump();
  xrpnet_dbg<0>.debug(str<>("Subscribing"), "orderbook xrpl:", currency_pair_string(cp));
  xrpnet_dbg<5>.debug(str<>("subscribe orderbook"), subscription);

  ws_orderbook = net::ws::qwebsocket_session::create("xrpl::orderbook" + currency_pair_string(cp),
      websocket_address(), websocket_port(), subscription,
      std::bind(xrpl_network::new_orderbook_data_q, this, cp, _1));

  return true;
}

// ----------------------------------------------------------------------------
bool xrpl_network::subscribe_accounts()
{
  using namespace std::placeholders;
  std::string addresses;
  for (auto const& w : subscribed_wallets_)
  {
    if (addresses.size()) addresses += ", ";
    addresses += "\"" + w.public_ + "\"";
  }
  std::string subscription = "{ \"command\": \"subscribe\", \"accounts\": [ " + addresses + " ] }";
  xrpnet_dbg<0>.debug(str<>("Subscribing"), "account changes for", addresses);

  ws_accounts =
      net::ws::qwebsocket_session::create("xrpl::accounts" + addresses, websocket_address(),
          websocket_port(), subscription, std::bind(xrpl_network::new_account_data_q, this, _1));

  return true;
}

// ----------------------------------------------------------------------------
void xrpl_network::new_orderbook_data_q(
    xrpl_network* exchange, currency_pair const cp, QString data)
{
  xrpnet_dbg<5>.debug(str<>("Orderbook"), "Ticker", currency_pair_string(cp));
  xrpnet_dbg<9>.debug(str<>("Orderbook data"), data.toStdString());

  // if shutdown was started after this data was sent by the remote source
  // then it can be ignored/dropped as we will not handle it anyway
  std::lock_guard l(exchange->async_mutex_);
  if (exchange->closing_down_)
  {
    xrpnet_dbg<0>.error(str<>("Orderbook data"), "Shutdown in progress: ignoring data");
    return;
  }

  auto process = [exchange, cp, data]() {
    const ticker_data tdata = exchange->get_subscribed_ticker_data(cp);
    try
    {
      std::string sdata = data.toStdString();
      json jdata = json::parse(sdata);
      if (jdata.contains("result") && jdata["result"].contains("offers"))
      {
        xrpnet_dbg<5>.debug(str<>("ledger_snapshot"));
        dynamic_pointer_cast<xrpl_order_book>(tdata->orderbook_)
            ->accept_json_ledger_snapshot(jdata["result"]["offers"]);
      }
      else if (jdata.contains("transaction") && jdata.contains("meta"))
      {
        xrpnet_dbg<5>.debug(str<>("ledger_transaction"));
        dynamic_pointer_cast<xrpl_order_book>(tdata->orderbook_)
            ->accept_json_ledger_transaction(jdata);
      }
      else
      {
        for (auto it = jdata.begin(); it != jdata.end(); it++)
          xrpnet_dbg<0>.error(str<>("Unrecognized"), "key: ", it.key(), it.value().dump(4));
      }
      //
      xrpnet_dbg<4>.debug(str<>("orderbook callback"), currency_pair_string(cp));
      tdata->orderbook_subscribers_.publish(cp);
    }
    catch (...)
    {
      xrpnet_dbg<0>.error(str<>("Orderbook error"), currency_pair_string(cp), tdata->orderbook_,
          data.toStdString());
    }
  };

  stdexec::sender auto snd =
      stdexec::on(default_pool_scheduler(), stdexec::just()) | stdexec::then(process);
  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
void xrpl_network::new_account_data_q(xrpl_network* nw, QString qdata)
{
  std::string data = qdata.toStdString();
  xrpnet_dbg<0>.debug(str<>("Account changes"), data);
  if (startswith(data, "{\"result\":"))
  {
    // ignore this, just a subscription ok
    xrpnet_dbg<5>.debug(str<>("Account subscription"), data);
  }
  else if (startswith(data, "{\"engine_result\":\"tesSUCCESS\""))
  {
    json jdata = json::parse(data);
    xrpnet_dbg<5>.debug(str<>("Account changes"), jdata.dump(4));
    json adata = jdata["meta"]["AffectedNodes"];
    for (auto const& a : adata)
    {
      //            try {
      xrpnet_dbg<0>.debug(str<>("AffectedNode"), jdata.dump(4));
      if (a.contains("ModifiedNode"))
      {
        auto m = a["ModifiedNode"];
        auto f = m["FinalFields"];
        auto p = m["PreviousFields"];
        auto b = f["Balance"];
        double oldb;
        double newb;
        //
        if (f.contains("Account"))
        {
          std::string acct = f["Account"].get<std::string>();
          oldb = 1E-6 * std::stod(p["Balance"].get_ptr<json::string_t*>()->c_str());
          newb = 1E-6 * std::stod(b.get_ptr<json::string_t*>()->c_str());
          std::cout << "Acct " << acct << " old balance " << oldb << " new balance " << newb
                    << std::endl;
          nw->update_XRP_balance(acct, oldb, newb);
        }
        // is this an IOU balance change?
        else if (b.contains("issuer") &&
            (b["issuer"].get<std::string>() == "rrrrrrrrrrrrrrrrrrrrBZbvji"))
        {
          auto t = jdata["transaction"];
          std::string fm_acct = t["Account"];
          std::string to_acct = t["Destination"];
          currency curr(
              t["SendMax"]["issuer"].get<std::string>(), b["currency"].get<std::string>());
          curr.balance_ = std::stod(b["value"].get_ptr<json::string_t*>()->c_str());
          if (to_acct == f["LowLimit"]["issuer"].get<std::string>())
          {
            std::cout << "Acct " << to_acct << " IOU balance change " << curr.balance_ << std::endl;
            nw->update_IOU_balance(to_acct, curr);
          }
          if (fm_acct == f["HighLimit"]["issuer"].get<std::string>())
          {
            curr.balance_ = -curr.balance_;
            std::cout << "Acct " << fm_acct << " IOU balance change " << curr.balance_ << std::endl;
            nw->update_IOU_balance(fm_acct, curr);
          }
        }
      }
      if (a.contains("DeletedNode"))
      {
        auto t = jdata["transaction"];
        if (t["TransactionType"].get<std::string>() == "OfferCancel")
        {
          std::string addr = t["Account"];
          std::uint64_t seq = t["OfferSequence"];
          auto it = nw->get_wallet_by_addr(addr);
          if (!it)
          {
            std::cerr << "Deleted node did not find wallet " << addr << std::endl;
            break;
          }
          it->delete_trade(seq);
          emit nw->transaction_event();
        }
      }
      //            }
      //            catch (...) {
      //                xrpnet_dbg<0>.debug(str<>("Exception Account changes : " << data);
      //            }
    }
  }
}

// ----------------------------------------------------------------------------
ledger_wallet* xrpl_network::get_wallet_by_addr(std::string_view addr)
{
  auto it = ranges::find_if(
      subscribed_wallets_, [addr](ledger_wallet const& w) { return w.public_ == addr; });
  if (it == subscribed_wallets_.end()) { return nullptr; }
  return &(*it);
}

// ----------------------------------------------------------------------------
ledger_wallet* xrpl_network::get_wallet_by_name(std::string_view name)
{
  auto it = ranges::find_if(
      subscribed_wallets_, [name](ledger_wallet const& w) { return w.name_ == name; });
  if (it == subscribed_wallets_.end()) { return nullptr; }
  return &(*it);
}

// ----------------------------------------------------------------------------
std::vector<currency>::iterator xrpl_network::get_currency(std::string_view addr, currency t)
{
  auto it = get_wallet_by_addr(addr);
  if (!it)
  {
    std::cerr << "get currency did not find acct " << addr << std::endl;
    return std::vector<currency>::iterator(nullptr);
  }
  auto& c_list = it->currencies_;
  auto it2 = ranges::find_if(c_list, [t](currency const& c) { return c == t; });
  if (it2 == c_list.end())
  {
    std::cerr << "get currency did not find ledger currency" << std::endl;
    return std::vector<currency>::iterator(nullptr);
  }
  return it2;
}

// ----------------------------------------------------------------------------
void xrpl_network::update_XRP_balance(std::string_view addr, double oldb, double newb)
{
  auto it = get_currency(addr, currency("", "XRP"));
  if (it == std::vector<currency>::iterator(nullptr)) return;
  //
  if (it->balance_ != oldb)
  {
    std::cerr << "Old balance error " << it->balance_ << " expected " << oldb << std::endl;
  }
  std::cerr << "Balance updated from " << oldb << " to " << newb << std::endl;
  it->balance_ = newb;
  it->avail_ = newb - it->reserved_;

  // signal GUI to update
  emit update_currency_widget(&(*it));
}

// ----------------------------------------------------------------------------
// an IOU update sets the new balance directly - it does not add/subtract
void xrpl_network::update_IOU_balance(std::string_view addr, currency const& curr)
{
  auto it = get_currency(addr, curr);
  if (it == std::vector<currency>::iterator(nullptr)) return;
  //
  double oldb = it->balance_;
  double newb = curr.balance_;
  std::cerr << "Balance updated from " << oldb << " to " << newb << std::endl;
  it->balance_ = newb;
  it->avail_ = newb - it->reserved_;

  // signal GUI to update
  emit update_currency_widget(&(*it));
}

// ----------------------------------------------------------------------------
any_bytearray_sender xrpl_network::submit_signed_transaction(std::string&& signed_tx)
{
  json tx;
  tx["tx_blob"] = signed_tx;

  json content;
  content["method"] = "submit";
  content["params"] = json::array({tx});

  // init an http request object
  std::string url = fmt::format("https://{}:{}", jsonrpc_address(), jsonrpc_port());
  xrpnet_dbg<5>.debug(str<>("signed_transaction"), url);
  auto* client = net::http::qhttp_request_client::create(
      *global_settings.networkmanager_, url, content.dump());
  return any_bytearray_sender{stdexec::just(client) | qhttp_post()};
}

// ----------------------------------------------------------------------------
any_bytearray_sender xrpl_network::get_account_lines(std::string addr)
{
  json params;
  params["account"] = addr;
  params["validated"] = true;

  json content;
  content["method"] = "account_lines";
  content["params"] = json::array({params});

  // init an http request object
  std::string url = fmt::format("https://{}:{}", jsonrpc_address(), jsonrpc_port());
  xrpnet_dbg<5>.debug(str<>("account_lines"), url);
  auto* client = net::http::qhttp_request_client::create(
      *global_settings.networkmanager_, url, content.dump());
  return any_bytearray_sender{stdexec::just(client) | qhttp_post()};
}

// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_lines(exec::async_scope& scope)
{
  for (auto& w : subscribed_wallets_)
  {
    auto snd = stdexec::on(QtStdExec::QThreadScheduler(), get_account_lines(w.public_))    // Qt
        | stdexec::then([this, &w](QByteArray byteArray) {                                 // pika
            std::string_view data(byteArray.constData(), byteArray.length());
            // debug : print the response headers and body
            xrpnet_dbg<8>.debug(str<>("Ledger response"), data);
            this->handle_account_lines(w, data);
          });
    scope.spawn(std::move(snd));
  }
}

// ----------------------------------------------------------------------------
void xrpl_network::handle_account_lines(ledger_wallet& w, std::string_view data)
{
  json jdata;
  try
  {
    jdata = json::parse(data)["result"]["lines"];
    if (jdata.size() == 0)
    {
      // no balances. Might be an inactive account
      return;
    }
  }
  catch (...)
  {
    xrpnet_dbg<0>.error(str<>("account trustline data error"));
    return;
  }

  xrpnet_dbg<8>.debug(str<>("account lines"), jdata.dump(4));
  std::vector<xrp_amount> balances = jdata.get<std::vector<xrp_amount>>();
  //
  for (auto const& b : balances)
  {
    if (!b.currency.is_xrp())
    {
      const currency_code ic = b.currency;
      if (ic.is_xrp())
      {
        currency c{ic, b.value, b.value, 0, nullptr};
        w.add_currency(c);
      }
      else if (ic.issuer_ == currency::bitstamp_trust)
      {
        currency c{ic, b.value, b.value, 0, nullptr};
        w.add_currency(c);
      }
      else if (ic.issuer_ == currency::gatehub_trust)
      {
        currency c{ic, b.value, b.value, 0, nullptr};
        w.add_currency(c);
      }
      else    // must be some other trustline balance
      {
        currency c{ic, b.value, b.value, 0, nullptr};
        w.add_currency(c);
        query_iou_fee(ic);
      }
    }
    else { throw std::runtime_error("Unknown currency in handle_account_balance"); }
  }
  w.compute_ledger_reserve();

  // signal GUI to update
  emit update_wallet_widget(&w);
}

// ----------------------------------------------------------------------------
any_bytearray_sender xrpl_network::get_account_info(std::string addr)
{
  json params;
  params["account"] = addr;
  params["ledger_index"] = "current";
  params["strict"] = true;
  params["queue"] = true;

  json content;
  content["method"] = "account_info";
  content["params"] = json::array({params});

  // init an http request object
  std::string url = fmt::format("https://{}:{}", jsonrpc_address(), jsonrpc_port());
  xrpnet_dbg<5>.debug(str<>("account_info"), url);
  auto* client = net::http::qhttp_request_client::create(
      *global_settings.networkmanager_, url, content.dump());
  return any_bytearray_sender{stdexec::just(client) | qhttp_post()};
}

// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_infos(exec::async_scope& scope)
{
  for (auto& w : subscribed_wallets_)
  {
    auto snd = stdexec::on(QtStdExec::QThreadScheduler(), get_account_info(w.public_)) |
        stdexec::then([this, &w](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          // debug : print the response headers and body
          xrpnet_dbg<8>.debug(str<>("account_info"), w.public_, data);
          this->handle_account_info(w, data);
        });
    scope.spawn(std::move(snd));
  }
}

// ----------------------------------------------------------------------------
void xrpl_network::handle_account_info(ledger_wallet& w, std::string_view data)
{
  json jdata;
  try
  {
    jdata = json::parse(data)["result"]["account_data"];
  }
  catch (...)
  {
    xrpnet_dbg<0>.error(str<>("account info data error"));
    return;
  }

  xrpnet_dbg<8>.debug(str<>("account info"), jdata.dump(4));
  //
  if (jdata.is_null()) return;
  //
  assert(w.public_ == jdata.at("Account").get<std::string>());
  w.sequence_ = jdata.at("Sequence").get<int32_t>();
  //
  std::string bal = jdata.at("Balance").get<std::string>();
  double balance = std::stod(bal) / 1E6;
  double avail = balance;
  double reserved = 0;
  //
  currency c{{"", "XRP"}, balance, avail, reserved, nullptr};
  w.add_currency(c);
  w.compute_ledger_reserve();
  //
  xrpnet_dbg<5>.debug(str<>("sequence"), w.public_, w.sequence_);
  // signal GUI to update
  emit update_wallet_widget(&w);
}

// ----------------------------------------------------------------------------
any_bytearray_sender xrpl_network::get_account_offers(std::string addr)
{
  json params;
  params["account"] = addr;

  json content;
  content["method"] = "account_offers";
  content["params"] = json::array({params});

  // init an http request object
  std::string url = fmt::format("https://{}:{}", jsonrpc_address(), jsonrpc_port());
  xrpnet_dbg<5>.debug(str<>("account_offers"), url);
  auto* client = net::http::qhttp_request_client::create(
      *global_settings.networkmanager_, url, content.dump());
  return any_bytearray_sender{stdexec::just(client) | qhttp_post()};
}

// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_offers(exec::async_scope& scope)
{
  for (auto& w : subscribed_wallets_)
  {
    auto snd = stdexec::on(QtStdExec::QThreadScheduler(), get_account_offers(w.public_)) |
        stdexec::then([this, &w](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          // debug : print the response headers and body
          xrpnet_dbg<8>.debug(str<>("account_offers"), w.public_, data);
          this->handle_account_offers(w, data);
        });
    scope.spawn(std::move(snd));
  };
}

// ----------------------------------------------------------------------------
void xrpl_network::handle_account_offers(ledger_wallet& w, std::string_view data)
{
  json jdata;
  try
  {
    jdata = json::parse(data)["result"];
    if (jdata.contains("error") && jdata.at("error") == "actNotFound")
    {
      xrpnet_dbg<0>.error(str<>("actNotFound"));
      return;
    }
  }
  catch (...)
  {
    xrpnet_dbg<0>.error(str<>("account offers data error"));
    return;
  }

  xrpnet_dbg<8>.debug(str<>("account offers"), jdata.dump(4));
  //
  if (jdata.is_null()) return;
  assert(w.public_ == jdata.at("account").get<std::string>());
  auto offers = jdata["offers"];
  if (offers.size() == 0) return;
  //
  w.offers_.clear();
  for (auto const& offer : offers)
  {
    //
    xrp_amount taker_get;
    xrp_amount taker_pay;
    grox::from_json(offer["taker_gets"], taker_get);
    grox::from_json(offer["taker_pays"], taker_pay);
    //
    trade_data t{get_instance(testnet()), w.name_, taker_pay.currency, taker_get.currency,
        taker_pay.value, taker_get.value,
        0.0,    // fee %
        0.0,    // fee fixed
        0,      // id
        offer.at("seq").get<std::uint64_t>(), "- no date -"};

    // xrp amounts are in drops, do divide by 1E6 to get whole xrp units
    if (t.get_trade_type() == trade_type::buy)
    {
      t.taker_pay_ /= 1E6;
      t.exchange_rate_ = t.taker_get_ / t.taker_pay_;
    }
    else if (t.get_trade_type() == trade_type::sell)
    {
      t.taker_get_ /= 1E6;
      t.exchange_rate_ = t.taker_pay_ / t.taker_get_;
    }
    //
    w.offers_.push_back(t);
  }
  // signal GUI to update
  emit update_wallet_widget(&w);
}

// ----------------------------------------------------------------------------
bool xrpl_network::make_payment(currency const& c, basic_account* src, basic_account* dest)
{
  ledger_wallet* from = static_cast<ledger_wallet*>(src);
  ledger_wallet* to = static_cast<ledger_wallet*>(dest);
  std::string signed_tx;
  // are we sending xrp or an IOU? xrp is always sent in drops
  if (c.is_xrp())
  {
    std::cout << "XRP payment amount " << c.balance_ << " from " << from->public_ << " to "
              << to->public_ << ((to->tag_ != 0) ? "(" + std::to_string(to->tag_) + ")" : "")
              << std::endl;

    signed_tx =
        make_xrp_payment(ripple::KeyType::secp256k1, from->private_, from->public_, from->sequence_,
            to->get_receive_address(c).begin(), to->tag_, c.balance_ * 1000000, "", "", 0.0);
  }
  else
  {
    double fee = get_transfer_fee(c);
    std::cout << "XRP IOU payment amount " << c.balance_ << " " << c.code_ << " TransferRate "
              << fee << " from " << from->public_ << " to " << to->public_ << " IOU addr "
              << c.issuer_ << ((to->tag_ != 0) ? "(" + std::to_string(to->tag_) + ")" : "")
              << std::endl;

    signed_tx =
        make_xrp_payment(ripple::KeyType::secp256k1, from->private_, from->public_, from->sequence_,
            to->get_receive_address(c).begin(), to->tag_, c.balance_, c.code_, c.issuer_, fee);
  }
  from->sequence_++;

  auto snd =
      stdexec::on(QtStdExec::QThreadScheduler(), submit_signed_transaction(std::move(signed_tx))) |
      stdexec::then([this](QByteArray byteArray) {
        std::string_view data(byteArray.constData(), byteArray.length());
        xrpnet_dbg<8>.debug(str<>("make_payment"), data);
        emit transaction_event();
      });
  stdexec::start_detached(std::move(snd));
  return true;
}

// ----------------------------------------------------------------------------
// place a buy/sell order
void xrpl_network::place_limit_order(basic_account* acct, trade_data const& t, bool update_after)
{
  using namespace ripple;
  //
  ledger_wallet* from = static_cast<ledger_wallet*>(acct);

  auto pay_pair = t.taker_payc_.to_string();
  auto get_pair = t.taker_getc_.to_string();
  ripple::STAmount taker_pays, taker_gets;

  // fiat currencies are multipled by 100 and shifted left by 2
  Currency curr_p = to_currency(pay_pair.first);
  if (t.taker_payc_.is_fiat())
  {
    auto const issuer = parseBase58<AccountID>(pay_pair.second);
    taker_pays = STAmount(Issue(curr_p, *issuer), static_cast<uint64_t>(1E2 * t.taker_pay_), -2);
  }
  // non fiat IOUs are multiplied by 1E6 and shifted right by 6 places
  else if (!t.taker_payc_.is_xrp())
  {
    auto const issuer = parseBase58<AccountID>(pay_pair.second);
    taker_pays = STAmount(Issue(curr_p, *issuer), static_cast<uint64_t>(1E6 * t.taker_pay_), -6);
  }
  // xrp is converted to drops by mutiplying by 1E6
  else if (t.taker_payc_.is_xrp())
  {
    taker_pays = STAmount(XRPAmount(1E6 * t.taker_pay_));    // drops
  }
  else { throw std::runtime_error("Unknown currency type taker_pays"); }

  // fiat currencies are multipled by 100 and shifted left by 2
  Currency curr_g = to_currency(get_pair.first);
  if (t.taker_getc_.is_fiat())
  {
    auto const issuer = parseBase58<AccountID>(get_pair.second);
    taker_gets = STAmount(Issue(curr_g, *issuer), static_cast<uint64_t>(1E2 * t.taker_get_), -2);
  }
  // non fiat IOUs are multiplied by 1E6 and shifted right by 6 places
  if (!t.taker_getc_.is_xrp())
  {
    auto const issuer = parseBase58<AccountID>(get_pair.second);
    taker_gets = STAmount(Issue(curr_g, *issuer), static_cast<uint64_t>(1E6 * t.taker_get_), -6);
  }
  else if (t.taker_getc_.is_xrp())
  {
    taker_gets = STAmount(XRPAmount(1E6 * t.taker_get_));    // drops
  }
  else { throw std::runtime_error("Unknown currency type taker_gets"); }

  // sign the transaction
  std::string signed_tx = make_xrp_offer(ripple::KeyType::secp256k1, from->private_, from->public_,
      from->sequence_, taker_pays, taker_gets, 0);
  from->sequence_++;

  auto snd =
      stdexec::on(QtStdExec::QThreadScheduler(), submit_signed_transaction(std::move(signed_tx))) |
      stdexec::then([this](QByteArray byteArray) {
        std::string_view data(byteArray.constData(), byteArray.length());
        xrpnet_dbg<8>.debug(str<>("place_limit_order"), data);
        emit transaction_event();
      });
  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
void xrpl_network::place_buy_sell_orders(basic_account* acct, std::vector<trade_data> const& trades)
{
  for (auto const& t : trades)
  {
    //last order in list triggers update
    if (&t == &trades.back())
      place_limit_order(acct, t, true);
    else
      place_limit_order(acct, t, false);
  }
}

// ----------------------------------------------------------------------------
void xrpl_network::cancel_order(trade_data const& t)
{
  ledger_wallet* from = get_wallet_by_name(t.wallet_);
  if (!from) { throw std::runtime_error("Cannot cancel order from " + t.wallet_); }

  // sign the transaction
  std::string signed_tx = cancel_xrp_offer(
      ripple::KeyType::secp256k1, from->private_, from->public_, from->sequence_, t.id_, 0);
  from->sequence_++;

  auto snd =
      stdexec::on(QtStdExec::QThreadScheduler(), submit_signed_transaction(std::move(signed_tx))) |
      stdexec::then([this](QByteArray byteArray) {
        std::string_view data(byteArray.constData(), byteArray.length());
        xrpnet_dbg<8>.debug(str<>("cancel_order"), data);
        emit transaction_event();
      });
  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
void xrpl_network::query_iou_fee(currency_code const& c1)
{
  if (currency_fees_.find(c1.issuer_) != currency_fees_.end())
  {
    // no need to fetch it twice
    return;
  }

  auto snd = stdexec::on(QtStdExec::QThreadScheduler(), get_account_info(c1.issuer_)) |
      stdexec::then([this, c1](QByteArray byteArray) {
        std::string_view data(byteArray.constData(), byteArray.length());
        // debug : print the response headers and body
        xrpnet_dbg<8>.debug(str<>("query_iou_fee"), c1.issuer_, data);
        json jdata = json::parse(data)["result"]["account_data"];
        if (jdata.contains("TransferRate"))
        {
          int sfee = jdata["TransferRate"].get<int>();
          // In the XRP Ledger protocol, the transfer fee is specified in the TransferRate
          // field, as an integer which represents the amount you must send for the
          // recipient to get 1 billion units of the same currency.
          // A TransferRate of 1005000000 is equivalent to a transfer fee of 0.5%
          double feepercent = 100.0 * (1E-9 * sfee - 1.0);
          xrpnet_dbg<0>.debug(str<>("fee %"), c1.issuer_, feepercent);
          currency_fees_[c1.issuer_] = feepercent;
        }
      });

  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
double xrpl_network::get_transfer_fee(currency const& c1) { return currency_fees_[c1.issuer_]; }

// ----------------------------------------------------------------------------
double xrpl_network::get_fee_percent(currency_pair const& cp) { return 0.0; }

// ----------------------------------------------------------------------------
double xrpl_network::get_fee_fixed(currency_pair const& cp) { return 0.0; }

// ----------------------------------------------------------------------------
void xrpl_network::trustline(
    basic_account* acct, std::string addr, std::string code, uint64_t limit, std::uint32_t flags)
{
  ledger_wallet* from = get_wallet_by_name(acct->name_);
  std::string signed_tx = set_trustline(ripple::KeyType::secp256k1, from->private_, from->public_,
      from->sequence_, limit, code, addr, flags);

  auto snd =
      stdexec::on(QtStdExec::QThreadScheduler(), submit_signed_transaction(std::move(signed_tx))) |
      stdexec::then([this](QByteArray byteArray) {
        std::string_view data(byteArray.constData(), byteArray.length());
        xrpnet_dbg<8>.debug(str<>("trustline"), data);
        emit transaction_event();
      });
  stdexec::start_detached(std::move(snd));
}

// ----------------------------------------------------------------------------
void xrpl_network::custom_functions(basic_account* acct)
{
  std::cout << "Network function " << std::endl;
  QDialog dlg;
  xrp_functions* f = new xrp_functions(this, acct, &dlg);

  QHBoxLayout* HLayout = new QHBoxLayout(&dlg);
  HLayout->addWidget(f);
  dlg.setLayout(HLayout);
  dlg.exec();
}

// ----------------------------------------------------------------------------
