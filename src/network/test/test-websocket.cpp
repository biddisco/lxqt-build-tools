//------------------------------------------------------------------------------
//
// Example: WebSocket SSL client, asynchronous
//
//------------------------------------------------------------------------------

#include <openssl/ssl.h>

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
//
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
//
#include "network/websocket-ssl.hpp"

//------------------------------------------------------------------------------
void new_trade_data(std::string&& data)
{
    std::cout << "\n\nReceived\n\n" << data << std::endl;
}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage  : bin/test-websocket <host> <port> <target> "
                     "[<HTTP version: 1.0 or 1.1(default)>]\n"
                  << "Example:\n"
                  << "bin/test-websocket ws.bitstamp.net 443 \"{\\\"event\\\": "
                     "\\\"bts:subscribe\\\",\\\"data\\\": {\\\"channel\\\": "
                     "\\\"live_trades_xrpusd\\\"}}\" \n"
            //            "\"/api/v2/ohlc/xrpusd/?step=60&limit=10\" \n" <<
            ;
        return EXIT_FAILURE;
    }

    auto const host = argv[1];
    auto const port = argv[2];
    auto const channel = argv[3];
    std::cout << "Connecting : " << host << ":" << port << " " << channel << "\n";
    int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;

    // The io_context is required for all I/O
    net::contexts contexts;

    // IO threads will terminate if there is no work, so we add a work_guard
    // to keep them alive until we want to exit.
    asio::executor_work_guard<asio::io_context::executor_type>
            work_guard_{boost::asio::make_work_guard(contexts.ioc)};

    std::vector<std::thread> threads_;
    // Run the I/O service on a thread.
    threads_.emplace_back([&]() {
        // The call will return when the socket is closed.
        contexts.ioc.run();
    });

    std::shared_ptr<net::ws::session> session = net::ws::create_session(
        contexts.ioc, contexts.ctx, host, port, channel, &new_trade_data);

    const int sec = 25;
    // wait N seconds and collect some data
    for (int i = 0; i < sec; i++)
    {
        std::cout << "Closing in " << sec - i << " seconds " << std::endl;
        std::chrono::seconds dura(1);
        std::this_thread::sleep_for(dura);
    }

    session->shutdown_blocking();
    work_guard_.reset();
    for (auto &t : threads_) {
        if (t.joinable()) t.join();
    }
    return EXIT_SUCCESS;
}
