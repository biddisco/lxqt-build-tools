#pragma once

#include <memory>
//
#include "currency/currency_pair.hpp"
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "data/order_book.hpp"
#include "exchange/abstract_exchange.hpp"
#include "exchange/ticker_streams.hpp"
#include "network/qwebsocket_session.hpp"

// ----------------------------------------------------------------------------
class price_chart_widget;
class order_book_base;

// ----------------------------------------------------------------------------
namespace ticker {
  struct subscription
  {
    std::shared_ptr<abstract_exchange> exchange_;
    std::shared_ptr<ohlc_dataset_view> view_;
    std::shared_ptr<order_book_base> orderbook_;
    std::shared_ptr<price_chart_widget> chart_widget_;
    // each ticker may subscribe to multiple streams
    std::map<ticker::streams, std::shared_ptr<net::ws::qwebsocket_session>> websockets_;
    //
    grox::PublishSubscribe<currency_pair const, grox::live_trade_data const>
        live_trade_subscribers_;
    grox::PublishSubscribe<currency_pair const> orderbook_subscribers_;
    grox::PublishSubscribe<candle_res const> new_ohlc_subscribers_;
  };

  using data = std::shared_ptr<subscription>;
}    // namespace ticker
