#pragma once

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

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace asio = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------


namespace net {

    // Report a failure
    void msg_fail(beast::error_code ec, char const* what);

    struct contexts
    {
        // The io_context is required for all I/O
        asio::io_context ioc;
        // The SSL context is required, and holds certificates
        ssl::context     ctx;

        // creat the objects we need
        contexts() : ioc(), ctx{ssl::context::tlsv12_client}
        {
            // use whatever OpenSSL certificate verification is provided by the OS
            ctx.set_default_verify_paths();

            // disallow older versions of SSL protocol/handshaking
            ctx.set_options( boost::asio::ssl::context::default_workarounds |
                    boost::asio::ssl::context::no_sslv2       |
                    boost::asio::ssl::context::no_sslv3       |
                    boost::asio::ssl::context::no_tlsv1       |
                    boost::asio::ssl::context::no_tlsv1_1
            );
        }
    };

    namespace ws {

        // encapsulate websocket and rest API handling here
        class session : public std::enable_shared_from_this<session>
        {
        public:
            using ssl_stream = beast::ssl_stream<beast::tcp_stream>;

            // address resolver
            tcp::resolver                   resolver_;

            // used by websocket connection
            websocket::stream<ssl_stream>   ws_;
            beast::flat_buffer              buffer_;
            std::string                     host_;
            std::string                     text_;

        public:
            //
            std::function<void(std::string &&)> read_callback;

        public:
            // Resolver and socket require an io_context
            explicit
            session(asio::io_context& ioc, ssl::context& ctx)
                : resolver_(asio::make_strand(ioc))
                , ws_(asio::make_strand(ioc), ctx)
            {
            }

            // Start the asynchronous operation
            void
            run(
                char const* host,
                char const* port,
                char const* text)
            {
                // Save these for later
                host_ = host;
                text_ = text;

                // Look up the domain name
                resolver_.async_resolve(
                    host,
                    port,
                    beast::bind_front_handler(
                        &session::on_resolve,
                        shared_from_this()));
            }

            void
            on_resolve(
                beast::error_code ec,
                tcp::resolver::results_type results)
            {
                if(ec)
                    return msg_fail(ec, "resolve");

                // Set the timeout for the operation
                beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

                // Make the connection on the IP address we get from a lookup
                beast::get_lowest_layer(ws_).async_connect(
                    results,
                    beast::bind_front_handler(
                        &session::on_connect,
                        shared_from_this()));
            }

            void
            on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
            {
                if(ec)
                    return msg_fail(ec, "connect");

                // Update the host_ string. This will provide the value of the
                // Host HTTP header during the WebSocket handshake.
                // See https://tools.ietf.org/html/rfc7230#section-5.4
                host_ += ':' + std::to_string(ep.port());

                // Set a timeout on the operation
                beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

                // Set SNI Hostname (many hosts need this to handshake successfully)
                if(! SSL_set_tlsext_host_name(
                        ws_.next_layer().native_handle(),
                        host_.c_str()))
                {
                    ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                        asio::error::get_ssl_category());
                    return msg_fail(ec, "connect");
                }

                // Perform the SSL handshake
                ws_.next_layer().async_handshake(
                    ssl::stream_base::client,
                    beast::bind_front_handler(
                        &session::on_ssl_handshake,
                        shared_from_this()));
            }

            void
            on_ssl_handshake(beast::error_code ec)
            {
                if(ec)
                    return msg_fail(ec, "ssl_handshake");

                // Turn off the timeout on the tcp_stream, because
                // the websocket stream has its own timeout system.
                beast::get_lowest_layer(ws_).expires_never();

                // Set suggested timeout settings for the websocket
                ws_.set_option(
                    websocket::stream_base::timeout::suggested(
                        beast::role_type::client));

                // Set a decorator to change the User-Agent of the handshake
                ws_.set_option(websocket::stream_base::decorator(
                    [](websocket::request_type& req)
                    {
                        req.set(http::field::user_agent,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                                " websocket-client-async-ssl");
                    }));

                // Perform the websocket handshake
                ws_.async_handshake(host_, "/",
                    beast::bind_front_handler(
                        &session::on_handshake,
                        shared_from_this()));
            }

            void
            on_handshake(beast::error_code ec)
            {
                if(ec)
                    return msg_fail(ec, "handshake");

                // Send the message
                ws_.async_write(
                    asio::buffer(text_),
                    beast::bind_front_handler(
                        &session::on_write,
                        shared_from_this()));
            }

            void
            write(std::string const &msg)
            {
                ws_.async_write(
                    asio::buffer(msg),
                    beast::bind_front_handler(
                        &session::on_write,
                        shared_from_this()));
            }

            void
            on_write(
                beast::error_code ec,
                std::size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);

                if(ec)
                    return msg_fail(ec, "write");

                // Read a message into our buffer
                ws_.async_read(
                    buffer_,
                    beast::bind_front_handler(
                        &session::on_read,
                        shared_from_this()));
            }

            void
            on_read(
                beast::error_code ec,
                std::size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);

                if(ec)
                    return msg_fail(ec, "read");

                // The make_printable() function helps print a ConstBufferSequence
                // std::cout << beast::make_printable(buffer_.data()) << std::endl;
                std::string str_buffer = beast::buffers_to_string(buffer_.data());
                buffer_.clear();

                // Read the next message
                ws_.async_read(
                    buffer_,
                    beast::bind_front_handler(
                        &session::on_read,
                        shared_from_this()));

                if (read_callback) {
                    read_callback(std::move(str_buffer));
                }
            }

            void
            on_close(beast::error_code ec)
            {
                if(ec)
                    return msg_fail(ec, "close");

                // If we get here then the connection is closed gracefully
                std::cout << "Connection closed gracefully" << std::endl;

                // The make_printable() function helps print a ConstBufferSequence
                std::cout << beast::make_printable(buffer_.data()) << std::endl;
            }

            void shutdown() {
                // Close the WebSocket connection
                ws_.async_close(websocket::close_code::normal,
                    beast::bind_front_handler(
                        &session::on_close,
                        shared_from_this()));
            }

            void shutdown_blocking() {
                beast::error_code ec;
                // Close the WebSocket connection
                ws_.close(websocket::close_code::normal, ec);
            }
        };

    std::shared_ptr<session> create_session(asio::io_context &ioc,
                                            ssl::context &ctx,
                                            std::string host,
                                            std::string port,
                                            std::string channel,
                                            std::function<void(std::string &&)> &&callback);
    }
}

