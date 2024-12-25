#pragma once

#include "currency/currency.hpp"
#include "data/ohlc_data_resolutions.hpp"
#include "data/order_book.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  struct arbitrage_decision
  {
  };

  //----------------------------------------------------------------------------
  class trade_arbitrage_2_way : public algorithm_base
  {
public:
    using operator_type = arbitrage_decision;

    // ---------------------------------------
    FACTORY_ALGORITHM_CREATE(trade_arbitrage_2_way);

    // ---------------------------------------
    /// Default constructor
    trade_arbitrage_2_way(std::string exchange = "Bistamp", currency_pair ticker = {})
      : algorithm_base("Arbitrage 2-way", "Arbitrage 2-way")
      , exchange_(exchange)
      , ticker_(ticker)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {                                                             //
          {"Samples", candle_data{ohlc_data_resolutions::minute15, 5000}},    //
          {"Window size", 14},                                                //
          {"mode", ohlc_modes::mid_open_close}};
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
