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
  class trade_arbitrage_2_way : public indicator_base
  {
public:
    using operator_type = arbitrage_decision;

    // ---------------------------------------
    FACTORY_INDICATOR_V2(trade_arbitrage_2_way)

    // ---------------------------------------
    /// Default constructor
    trade_arbitrage_2_way(std::string abstract_exchange = "Bistamp", currency_pair ticker = {})
      : indicator_base("Arbitrage 2-way", "Arbitrage 2-way", {overlay_type::no_overlay})
      , exchange_(abstract_exchange)
      , ticker_(ticker)
    {
    }

    // ---------------------------------------
    indicator_kind kind() const override { return indicator_kind::orderbook; }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<order_book_param>{"Order-Book-1", {"Bitstamp", {{{"XRP"}, {"USD"}}}}},      // 0
          param<order_book_param>{"Order-Book-2",                                           //
              {"XRPL Mainnet", {{{"XRP"}, {currency_issuers::bitstamp_trust, "USD"}}}}},    // 1
          param<int>{"Window size", 14},                                                    // 2
          param<ohlc_modes>{"mode", ohlc_modes::mid_open_close},                            // 3
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override {}

    // ---------------------------------------
    sample_result process_sample(market_sample const& /*sample*/) override
    {
      // Stub: arbitrage logic will be implemented in a future phase
      return 0.0;
    }

private:
    std::string exchange_;
    currency_pair ticker_;
  };

}    // namespace indicators
