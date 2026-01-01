#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <boost/beast/ssl.hpp>
#include "boost/asio.hpp"
#include "boost/beast.hpp"
//
#include <curl/curl.h>
#include <uuid/uuid.h>
//
#include "network/evp-encrypt.hpp"
#include "network/test/https-async.hpp"
#include "util/execute_os_command.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

static std::atomic<int> reply_ready = 0;
static std::string api_user;
static std::string api_key;
static std::string api_secret;

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

  //
  api_user = execute_os_command("pass bitstamp/user");
  api_key = execute_os_command("pass bitstamp/account_key_main");
  api_secret = execute_os_command("pass bitstamp/account_sec_main");
  if ((api_user.find("Error") != std::string::npos) ||
      (api_key.find("Error") != std::string::npos) ||
      (api_secret.find("Error") != std::string::npos))
  {
    std::cout << "Set ENV vars for API_KEY and API_SEC " << std::endl;
    return EXIT_FAILURE;
  }

  secure_string randbytes = generate_random_alphanumeric_string(encryption::KEY_SIZE, 81192);
  encryption encryptor(api_secret, randbytes);

  std::chrono::milliseconds timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());

  std::string http_method = "POST";
  std::string url_host = "www.bitstamp.net";
  std::string url_path = argv[1];           // "/api/v2/user_transactions/";
  std::string url_query = argv[2];          // "?limit=2";
  std::string url_redirected = url_path;    //+ url_query;

  std::string x_auth = "BITSTAMP " + api_key;
  std::string x_auth_nonce = encryptor.generate_uuid_string();
  std::string x_auth_timestamp = std::to_string(timestamp.count());
  std::string x_auth_version = "v2";
  std::string content_type = "application/x-www-form-urlencoded";
  std::string payload = url_query.size() > 0 ? url_query : url_encode("{offset:1}");

  // full query is signed using Hmac SHA256 algorithm
  std::string data_to_sign = "";
  data_to_sign.append(x_auth);
  data_to_sign.append(http_method);
  data_to_sign.append(url_host);
  data_to_sign.append(url_path);
  data_to_sign.append("");    // url_query);
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

  std::shared_ptr<net::https::session> session =
      net::https::create_session(contexts.ioc, contexts.ctx, url_host, "443", bitstamp_reply);

  // Run the I/O service on a thread.
  std::thread io_thread([&]() {
    // The call will return when the socket is closed.
    contexts.ioc.run();
  });

  // wait until connection is setup
  while (!session->ready_) { std::this_thread::yield(); }

  // invoke a post on the context thread
  contexts.ioc.post([&]() {
    std::cout << "IO context::post ok" << std::endl;
    session->write(/*std::move(*/ request /*)*/);
  });

  int const sec = 5;
  // wait 5 seconds and collect some data
  for (int i = 0; i < sec && (reply_ready.load() == 0); i++)
  {
    std::cout << "Closing in " << sec - i << " seconds " << std::endl;
    std::chrono::seconds dura(1);
    std::this_thread::sleep_for(dura);
  }

  session->shutdown_blocking();
  session.reset();
  std::cout << "Completed " << std::endl;
  //
  contexts.work_guard_->reset();
  contexts.ioc.stop();
  io_thread.join();

  std::cout << "Exiting" << std::endl;
  return (reply_ready.load() > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
