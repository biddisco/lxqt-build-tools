#pragma once

#include <string>
//
#include "currency/currency.hpp"
#include "data/ohlc_data_resolutions.hpp"
#include "data/order_book.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
namespace indicators {

  //----------------------------------------------------------------------------
  class trade_currency_exchange : public indicator_base
  {
public:
    using operator_type = arbitrage_decision;

    // ---------------------------------------
    FACTORY_INDICATOR_V2(trade_currency_exchange)

    // ---------------------------------------
    /// Default constructor
    trade_currency_exchange(std::string abstract_exchange = "Bistamp", currency_pair ticker = {})
      : indicator_base("Currency-Exchange", "Currency-Exchange using live orderbooks",
            {overlay_type::no_overlay})
      , exchange_(abstract_exchange)
      , ticker1_(ticker)
    {
    }

    // ---------------------------------------
    indicator_kind kind() const override { return indicator_kind::orderbook; }

    // ---------------------------------------
    std::optional<supported_trade_actions> trade_action() const override
    {
      return supported_trade_actions::currency_exchange;
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<order_book_param>{
              "Order-Book-1", {"Bitstamp", {{{"XRP"}, {"USD"}}, {{"XRP"}, {"EUR"}}}}},    // 0
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override {}

    // ---------------------------------------
    sample_result process_sample(market_sample const& /*sample*/) override
    {
      // Stub: currency exchange logic will be implemented in a future phase
      return 0.0;
    }

private:
    std::string exchange_;
    currency_pair ticker1_;
    currency_pair ticker2_;
  };

}    // namespace indicators
