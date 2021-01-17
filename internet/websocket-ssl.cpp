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
// Example: WebSocket SSL client, asynchronous
//
//------------------------------------------------------------------------------

#include <openssl/ssl.h>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
//
#include "internet/websocket-ssl.hpp"

//------------------------------------------------------------------------------

namespace net {

    // Report a failure
    void msg_fail(beast::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << "\n";
    }

    namespace ws {

    std::shared_ptr<session> create_session(asio::io_context &ioc,
                                            ssl::context &ctx,
                                            std::string host,
                                            std::string port,
                                            std::string channel,
                                            std::function<void(std::string &&)> &&callback)
    {
        // Launch the asynchronous operation
        auto session_ptr = std::make_shared<session>(ioc, ctx);

        session_ptr->read_callback = std::move(callback);

        session_ptr->run(host.c_str(), port.c_str(), channel.c_str());

        return session_ptr;
    }
}}

