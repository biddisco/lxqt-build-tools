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
// Example: HTTP SSL client, asynchronous
//
//------------------------------------------------------------------------------

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
//
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

// just for namespaces etc
#include "network/test/websocket-ssl.hpp"

//------------------------------------------------------------------------------
namespace net { namespace https {

  // Performs an HTTP GET and prints the response
  class session : public std::enable_shared_from_this<session>
  {
    tcp::resolver resolver_;
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
    boost::beast::flat_buffer buffer_;    // (Must persist between reads)
    http::request<http::empty_body> req_;
    http::response<http::string_body> res_;

    std::string host_;
    std::string port_;
    std::string target_;

public:
    //
    using callback_type = std::function<void(std::string&&)>;
    callback_type read_callback_;
    std::atomic<bool> ready_;

    void set_callback(callback_type cb) { read_callback_ = cb; }

public:
    explicit session(asio::io_context& ioc, ssl::context& ctx)
      : resolver_(ioc)
      , stream_(ioc, ctx)
      , ready_(false)
    {
    }

    // Start the asynchronous operation
    void run(char const* host, char const* port)
    {
      // Set SNI Hostname (many hosts need this to handshake successfully)
      if (!SSL_set_tlsext_host_name(stream_.native_handle(), host))
      {
        boost::beast::error_code ec{
            static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
      }
      host_ = host;
      port_ = port;

      // these params don't change so set them here at startup
      req_.version(11);
      req_.method(http::verb::get);
      req_.set(http::field::host, host);
      req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

      // Look up the domain name
      resolver_.async_resolve(
          host, port, boost::beast::bind_front_handler(&session::on_resolve, shared_from_this()));
    }

    void on_resolve(boost::beast::error_code ec, tcp::resolver::results_type results)
    {
      if (ec) return msg_fail(ec, "resolve");

      std::cout << "Resolve ok" << std::endl;

      // Set a timeout on the operation
      boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

      // Make the connection on the IP address we get from a lookup
      boost::beast::get_lowest_layer(stream_).async_connect(
          results, boost::beast::bind_front_handler(&session::on_connect, shared_from_this()));
    }

    void on_connect(boost::beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
      if (ec) return msg_fail(ec, "connect");

      std::cout << "Connect ok" << std::endl;

      // Perform the SSL handshake
      stream_.async_handshake(ssl::stream_base::client,
          boost::beast::bind_front_handler(&session::on_handshake, shared_from_this()));
    }

    void on_handshake(boost::beast::error_code ec)
    {
      if (ec) return msg_fail(ec, "handshake");

      std::cout << "Handshake ok" << std::endl;

      ready_ = true;
    }

    void write(std::string target, unsigned version = 11)
    {
      if (!ready_) { throw std::runtime_error("Still processing last request"); }

      ready_ = false;
      // Set a timeout on the operation
      boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

      // Set up HTTP GET request, hold onto the request string (async=ownership)
      target_ = std::move(target);
      std::cout << "https: writing: " << host_ << ":" << target_ << std::endl;
      req_.target(target_);

      // Send the HTTP request to the remote host
      http::async_write(
          stream_, req_, boost::beast::bind_front_handler(&session::on_write, shared_from_this()));
    }

    void write(http::request<http::string_body>& request)
    {
      if (!ready_) { throw std::runtime_error("Still processing last request"); }

      ready_ = false;
      // Set a timeout on the operation
      boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

      std::cout << "https: writing: " << host_ << ":" << request << std::endl;

      // Send the HTTP request to the remote host
      http::async_write(stream_, request,
          boost::beast::bind_front_handler(&session::on_write, shared_from_this()));
    }

    void on_write(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);

      if (ec) return msg_fail(ec, "write");

      std::cout << "Write ok" << std::endl;

      // Receive the HTTP response
      http::async_read(stream_, buffer_, res_,
          boost::beast::bind_front_handler(&session::on_read, shared_from_this()));
    }

    void on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);

      if (ec) return msg_fail(ec, "read");

      std::cout << "Read ok" << std::endl;

      // The make_printable() function helps print a ConstBufferSequence
      // std::cout << boost::beast::make_printable(res_.data()) << std::endl;
      std::string str_buffer = res_.body();
      res_.body().clear();
      buffer_.clear();

      // safe to process the next request
      ready_ = true;

      // trigger the user callback
      if (read_callback_) { read_callback_(std::move(str_buffer)); }
    }

    void shutdown()
    {
      // Gracefully close the stream
      stream_.async_shutdown(
          boost::beast::bind_front_handler(&session::on_shutdown, shared_from_this()));
    }

    void shutdown_blocking()
    {
      // Close the htttps connection
      stream_.shutdown();
    }

    void on_shutdown(boost::beast::error_code ec)
    {
      if (ec == asio::error::eof)
      {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec = {};
      }
      if (ec) return msg_fail(ec, "shutdown");

      std::cout << "https: shutdown complete" << std::endl;
    }
  };

  inline std::shared_ptr<session> create_session(asio::io_context& ioc, ssl::context& ctx,
      std::string host, std::string port, std::function<void(std::string&&)>&& callback)
  {
    // Launch the asynchronous operation
    auto session_ptr = std::make_shared<session>(ioc, ctx);

    session_ptr->set_callback(std::move(callback));

    session_ptr->run(host.c_str(), port.c_str());

    return session_ptr;
  }

}}    // namespace net::https
