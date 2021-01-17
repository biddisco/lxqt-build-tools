//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP SSL client, asynchronous
//
//------------------------------------------------------------------------------

#include <openssl/ssl.h>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
//
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
//
#include "internet/https-async.hpp"

// Report a failure
namespace net {
    void msg_fail(beast::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << "\n";
    }
}

//------------------------------------------------------------------------------
void new_data(std::string &&data)
{
    std::cout << "\n\nReceived\n\n" << data << std::endl;
}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 4 && argc != 5)
    {
        std::cerr <<
            "Usage  : bin/test-https <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
            "Example:\n" <<
            "bin/test-https www.bitstamp.net 443 \"/api/v2/ohlc/xrpusd/?step=60&limit=10\" \n";
        return EXIT_FAILURE;
    }

    auto const host = argv[1];
    auto const port = argv[2];
    auto const target = argv[3];
    std::cout << "Connecting : " << host << ":" << port << " " << target << "\n";
    int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;

    // The io_context is required for all I/O
    net::contexts contexts;

    std::shared_ptr<net::https::session> session = net::https::create_session(
                contexts.ioc,
                contexts.ctx,
                host,
                port,
                new_data);

    // Run the I/O service on a thread.
    std::thread websocket_thread([&]()
        {
            // The call will return when the socket is closed.
            contexts.ioc.run();
        }
    );

    while(!session->ready_) { std::this_thread::yield(); }
    contexts.ioc.post([&](){ session->write(target, version); });

    // wait 5 seconds and collect some data
    for (int i=0; i<5; i++) {
        std::cout << "Closing in " << 5-i << " seconds " << std::endl;
        std::chrono::seconds dura(1);
        std::this_thread::sleep_for(dura);
    }

    session->shutdown_blocking();
    websocket_thread.join();

    return EXIT_SUCCESS;
}
