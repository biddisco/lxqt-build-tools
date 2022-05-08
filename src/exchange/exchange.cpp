#include <QObject>
//
#include "src/exchange/exchange.hpp"

bool exchange::websocket_enabled(network::streams s)
{
    if (enabled_streams_.find(s)!=enabled_streams_.end()) {
        return enabled_streams_[s];
    }
    return false;
}

void exchange::websocket_enable(network::streams s, net::contexts &io_contexts, bool enable)
{
    if (enable) {
        if (!websocket_enabled(s)) {
            enabled_streams_[s] = connect(io_contexts, {s});
        }
    }
    else {
        if (websocket_enabled(s)) {
            enabled_streams_[s] = !disconnect(io_contexts, {s});
        }
    }
}

