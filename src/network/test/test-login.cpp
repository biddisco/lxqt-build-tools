#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include <boost/beast/ssl.hpp>
#include "boost/asio.hpp"
#include "boost/beast.hpp"
//
#include <curl/curl.h>
#include <uuid/uuid.h>
//
#include "src/network/evp-encrypt.hpp"
#include "src/network/https-async.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Report a failure
namespace net {
    void msg_fail(beast::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << "\n";
    }
}    // namespace net

static std::atomic<int> reply_ready = 0;

void bitstamp_reply(std::string&& data)
{
    std::cout << "Response : " << data << std::endl;
    reply_ready = 1;
}

int main(int argc, char** argv)
{
    // Check command line arguments.
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage  : bin/test-login "
                  << "/api/v2/user_transactions/ "
                  << "\"?&limit=2\"" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string api_key = std::getenv("rand2") ? std::getenv("rand2") : "";
    const std::string api_secret = std::getenv("rand3") ? std::getenv("rand3") : "";
    if (api_key.empty() || api_secret.empty())
    {
        std::cout << "Set ENV vars for API_KEY and APi_SEC " << std::endl;
        return EXIT_FAILURE;
    }

    secure_string randbytes = "asd23asd234gf576cbv";
    encryption encryptor(api_secret, randbytes);

    std::chrono::milliseconds timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());

    std::string http_method         = "POST";
    std::string url_host            = "www.bitstamp.net";
    std::string url_path            = argv[1];    // "/api/v2/user_transactions/";
    std::string url_query           = argv[2];    // "?limit=2";
    std::string url_redirected      = url_path;   //+ url_query;

    std::string x_auth              = "BITSTAMP " + api_key;
    std::string x_auth_nonce        = encryptor.generate_uuid_string();
    std::string x_auth_timestamp    = std::to_string(timestamp.count());
    std::string x_auth_version      = "v2";
    std::string content_type        = "application/x-www-form-urlencoded";
    std::string payload             = url_query.size()>0 ? url_query : url_encode("{offset:1}");

    // full query is signed using Hmac SHA256 algorithm
    std::string data_to_sign = "";
    data_to_sign.append(x_auth);
    data_to_sign.append(http_method);
    data_to_sign.append(url_host);
    data_to_sign.append(url_path);
    data_to_sign.append(""); // url_query);
    data_to_sign.append(content_type);
    data_to_sign.append(x_auth_nonce);
    data_to_sign.append(x_auth_timestamp);
    data_to_sign.append(x_auth_version);
    data_to_sign.append(payload);

    // generated signature
    auto signed_hmac = encryptor.CalcHmacSHA256(api_secret, data_to_sign);
    assert(signed_hmac.size() == 32);
    std::string x_auth_signature = b2a_hex(signed_hmac.data(), signed_hmac.size());

    http::request<http::string_body> request(http::verb::post, url_redirected, 11);
    request.set(http::field::host, url_host);
    request.set(http::field::content_type, content_type);
    request.set("X-Auth", x_auth);
    request.set("X-Auth-Nonce", x_auth_nonce);
    request.set("X-Auth-Timestamp", x_auth_timestamp);
    request.set("X-Auth-Version", x_auth_version);
    request.set("X-Auth-Signature", x_auth_signature);
    //
    request.body() = payload;
    request.prepare_payload();

    // The io_context is required for all I/O
    net::contexts contexts;

    std::shared_ptr<net::https::session> session = net::https::create_session(
        contexts.ioc, contexts.ctx, url_host, "443", bitstamp_reply);

    // Run the I/O service on a thread.
    std::thread websocket_thread([&]() {
        // The call will return when the socket is closed.
        contexts.ioc.run();
    });

    // wait until connection is setup
    while (!session->ready_)
    {
        std::this_thread::yield();
    }

    // invoke a post on the context thread
    contexts.ioc.post([&]() {
        std::cout << "IO context::post ok" << std::endl;
        session->write(/*std::move(*/ request /*)*/);
    });

    // wait 5 seconds and collect some data
    for (int i = 0; i < 5 && reply_ready < 1; i++)
    {
        std::cout << "Closing in " << 5 - i << " seconds " << std::endl;
        std::chrono::seconds dura(1);
        std::this_thread::sleep_for(dura);
    }

    session->shutdown_blocking();
    websocket_thread.join();
    //
    return EXIT_SUCCESS;
}
