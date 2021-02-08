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
// Example: HTTP client, asynchronous
//
//------------------------------------------------------------------------------

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "src/internet/https-async.hpp"

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

namespace net { namespace https {

    std::shared_ptr<session> create_session(asio::io_context& ioc, ssl::context& ctx,
        std::string host, std::string port, std::function<void(std::string&&)>&& callback)
    {
        // Launch the asynchronous operation
        auto session_ptr = std::make_shared<session>(ioc, ctx);

        session_ptr->read_callback = std::move(callback);

        session_ptr->run(host.c_str(), port.c_str());

        return session_ptr;
    }
}}    // namespace net::https
