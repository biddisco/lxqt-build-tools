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
  class trade_market_maker : public indicator_base
  {
public:
    using operator_type = arbitrage_decision;

    // ---------------------------------------
    FACTORY_INDICATOR_V2(trade_market_maker)

    // ---------------------------------------
    /// Default constructor
    trade_market_maker(std::string abstract_exchange = "Bistamp", currency_pair ticker = {})
      : indicator_base("Market-Maker", "Default market maker instance", {overlay_type::no_overlay})
      , exchange_(abstract_exchange)
      , ticker_(ticker)
    {
    }

    // ---------------------------------------
    indicator_kind kind() const override { return indicator_kind::orderbook; }

    // ---------------------------------------
    std::optional<supported_trade_actions> trade_action() const override
    {
      return supported_trade_actions::market_maker;
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<order_book_param>{"Order-Book-1", {"Bitstamp", {{{"XRP"}, {"USD"}}}}},    // 0
          param<int>{"Num Spreads", 5},                                                   // 1
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override {}

    // ---------------------------------------
    sample_result process_sample(market_sample const& /*sample*/) override
    {
      // Stub: market making logic will be implemented in a future phase
      return 0.0;
    }

private:
    std::string exchange_;
    currency_pair ticker_;
  };

}    // namespace indicators
