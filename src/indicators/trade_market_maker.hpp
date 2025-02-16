#pragma once

#include <string>
//
#include "currency/currency.hpp"
#include "data/ohlc_data_resolutions.hpp"
#include "data/order_book.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
#define FACTORY_ARBITRAGE_CREATE(type)                                                             \
  FACTORY_ALGORITHM_CREATE(type)                                                                   \
  static inline arbitrage_type_inserter<type> inserter{};

// ----------------------------------------------------------------------------
namespace indicators {

  //----------------------------------------------------------------------------
  class trade_market_maker : public algorithm_base
  {
public:
    using operator_type = arbitrage_decision;

    // ---------------------------------------
    FACTORY_ARBITRAGE_CREATE(trade_market_maker);

    // ---------------------------------------
    /// Default constructor
    trade_market_maker(std::string abstract_exchange = "Bistamp", currency_pair ticker = {})
      : algorithm_base("Market-Maker", "Default market maker instance")
      , exchange_(abstract_exchange)
      , ticker_(ticker)
    {
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
    double operator()(ohlctv_sample const& ohlc) { return 0.0; }

private:
    std::string exchange_;
    currency_pair ticker_;
  };

}    // namespace indicators
