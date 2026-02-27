#pragma once

#include <string>
//
#include "currency/currency.hpp"
#include "data/ohlc_data_resolutions.hpp"
#include "data/order_book.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

// Macro for trading algorithm factory creation
// Note: Registration is handled by plugins, not via static initializers
#define FACTORY_ARBITRAGE_CREATE(type) FACTORY_ALGORITHM_CREATE(type)

// ----------------------------------------------------------------------------
namespace indicators {

  //----------------------------------------------------------------------------
  class trade_currency_exchange : public algorithm_base
  {
public:
    using operator_type = arbitrage_decision;

    // ---------------------------------------
    FACTORY_ARBITRAGE_CREATE(trade_currency_exchange);

    // ---------------------------------------
    /// Default constructor
    trade_currency_exchange(std::string abstract_exchange = "Bistamp", currency_pair ticker = {})
      : algorithm_base("Currency-Exchange", "Currency-Exchange using live orderbooks")
      , exchange_(abstract_exchange)
      , ticker1_(ticker)
    {
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
    double operator()(ohlctv_sample const& ohlc) { return 0.0; }

private:
    std::string exchange_;
    currency_pair ticker1_;
    currency_pair ticker2_;
  };

}    // namespace indicators
