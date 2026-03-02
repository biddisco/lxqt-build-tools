#pragma once

#include <set>
#include <string>
//
#include <magic_enum/magic_enum.hpp>
//
#include "currency/currency_pair.hpp"
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "data/order_book.hpp"
#include "exchange/abstract_exchange.hpp"
#include "network/qwebsocket_session.hpp"

// ----------------------------------------------------------------------------
namespace ticker {
  enum streams : int
  {
    my_trades = 0,
    my_orders,
    live_trades,
    order_book,
    price_data,
    account_changes,
    invalid,
  };
  constexpr auto stream_names = magic_enum::enum_names<ticker::streams>();

  struct transaction_fees
  {
    double maker_percent;
    double taker_percent;
    double fixed;
    double transfer_percent;
  };
}    // namespace ticker

using stream_set = std::set<ticker::streams>;

// ----------------------------------------------------------------------------
inline std::string stream_to_pretty_text(ticker::streams stream)
{
  std::string txt = std::string(magic_enum::enum_name(stream));
  // Transform fir char after each break
  txt[0] = std::toupper(txt[0]);
  std::for_each(txt.begin() + 1, txt.end(), [](char& c) {
    if ((*(&c - 1)) == '_') c = std::toupper(c);
  });
  std::transform(txt.begin(), txt.end(), txt.begin(), [](char& c) { return (c == '_') ? ' ' : c; });
  return txt;
}

inline ticker::streams stream_from_pretty_text(std::string txt)
{
  std::transform(txt.begin(), txt.end(), txt.begin(), [](char c) { return std::tolower(c); });
  auto stream = magic_enum::enum_cast<ticker::streams>(txt);
  if (stream.has_value()) { return stream.value(); }
  return ticker::streams::invalid;
}
