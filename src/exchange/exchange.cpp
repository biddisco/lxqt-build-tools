#include <QObject>
//
#include "src/print.hpp"
#include "src/exchange/exchange.hpp"
#include <range/v3/algorithm.hpp>

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> exchange_dbg("Exchange");

// ----------------------------------------------------------------------------
bool exchange::websocket_enabled(network::streams s)
{
    if (enabled_streams_.find(s)!=enabled_streams_.end()) {
        return enabled_streams_[s];
    }
    return false;
}

// ----------------------------------------------------------------------------
void exchange::websocket_enable(network::streams s, net::contexts &io_contexts, bool enable)
{
    if (enable) {
        if (!websocket_enabled(s)) {
            enabled_streams_[s] = websocket_connect(io_contexts, {s});
        }
    }
    else {
        if (websocket_enabled(s)) {
            enabled_streams_[s] = !websocket_disconnect(io_contexts, {s});
        }
    }
}

// ----------------------------------------------------------------------------
const currency_pairlist & exchange::get_currency_pairs() {
    return tickers_available_;
}

// ----------------------------------------------------------------------------
const exchange::exchange_map & exchange::tickers_subscribed()
{
    return tickers_subscribed_;
}

// ----------------------------------------------------------------------------
bool exchange::ticker_subscribed(const currency &c1, const currency &c2)
{
    auto present = (tickers_subscribed_.contains(currency_pair{c1,c2}));
    return present;
}

// ----------------------------------------------------------------------------
bool exchange::ticker_subscribed(std::string_view p1, std::string_view p2)
{
    currency c1 = ::get_currency(p1);
    currency c2 = ::get_currency(p2);
    return ticker_subscribed(c1,c2);
}

// ----------------------------------------------------------------------------
void exchange::ticker_subscribe(const currency &c1, const currency &c2)
{
    throw std::runtime_error("Exchange classes must implement this function");
}

// ----------------------------------------------------------------------------
void exchange::ticker_subscribe(std::string_view p1, std::string_view p2)
{
    currency c1 = ::get_currency(p1);
    currency c2 = ::get_currency(p2);
    ticker_subscribe(c1,c2);
}
