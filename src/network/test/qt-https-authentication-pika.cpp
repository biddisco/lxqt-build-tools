#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
//
#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QTimer>
//
#include <pika/init.hpp>
#include <pika/modules/execution.hpp>
#include <pika/modules/executors.hpp>
#include <pika/modules/resource_partitioner.hpp>
#include <pika/modules/schedulers.hpp>
#include <pika/modules/thread_manager.hpp>
#include <pika/program_options.hpp>
#include "fmt/format.h"
//
#include "debug/print.hpp"
#include "network/evp-encrypt.hpp"
#include "network/qhttp-request-client.hpp"
#include "util/execute_os_command.hpp"

static std::atomic<int> reply_ready = 0;
static std::string api_user;
static std::string api_key;
static std::string api_secret;
const std::string bitstamp_https_address = "www.bitstamp.net";
const int bitstamp_https_port = 443;

// ----------------------------------------------------------------------------
using namespace grox::debug;
//std::getenv("rand2") ? std::getenv("rand2") : ""
template <int Level>
static print_threshold<Level, 2> test1_dbg("https://");

// ----------------------------------------------------------------------------
void account_request(QNetworkAccessManager& networkmanager_, const std::string& url_path,
  const std::string& url_query, net::http::rx_req_handler_type&& handler)
{
  secure_string randbytes = generate_random_alphanumeric_string(encryption::KEY_SIZE, 81192);
  encryption encryptor(api_key, randbytes);
  //
  std::chrono::milliseconds timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch());

  // setup REST request fields
  std::string url_host = bitstamp_https_address;
  std::string content_type = "application/x-www-form-urlencoded";
  std::string payload = url_query.size() > 0 ? url_query : url_encode("{offset:1}");
  std::string http_method = "POST";
  std::string x_auth = "BITSTAMP " + api_key;
  std::string x_auth_nonce = encryptor.generate_uuid_string();
  std::string x_auth_timestamp = std::to_string(timestamp.count());
  std::string x_auth_version = "v2";

  // https://www.bitstamp.net/api/#section/Authentication
  // x_auth_signature:
  //   sha256.hmac({string_to_sign}, {api_secret})
  //   {string_to_sign} is your signature message.
  //   Content-Type should not be added to the string if request.body is empty.
  //   The following have to be combined into a single string:
  //   "BITSTAMP" + " " + api_key +
  //   HTTP Verb +
  //   url.host +
  //   url.path +
  //   url.query +
  //   Content-Type +
  //   X-Auth-Nonce +
  //   X-Auth-Timestamp +
  //   X-Auth-Version +
  //   request.body

  std::string string_to_sign = "";
  string_to_sign.append(x_auth);
  string_to_sign.append(http_method);
  string_to_sign.append(url_host);
  string_to_sign.append(url_path);
  string_to_sign.append(url_query);
  string_to_sign.append(payload.size() > 0 ? content_type.c_str() : "");
  string_to_sign.append(x_auth_nonce);
  string_to_sign.append(x_auth_timestamp);
  string_to_sign.append(x_auth_version);
  string_to_sign.append(payload);

  // generated signature
  auto signed_hmac = encryptor.CalcHmacSHA256(api_secret, string_to_sign);
  assert(signed_hmac.size() == 32);
  std::string x_auth_signature = b2a_hex(signed_hmac.data(), signed_hmac.size());

  std::string urlstring = fmt::format(
    "https://{}:{}{}{}", bitstamp_https_address, bitstamp_https_port, url_path, url_query);

  QNetworkRequest request(QUrl(urlstring.c_str()));
  request.setRawHeader("Content-Type", content_type.c_str());
  request.setRawHeader("User-Agent", "mystery");
  request.setRawHeader("Accept", "application/json");
  request.setRawHeader("Connection", "close");
  //
  request.setRawHeader("X-Auth", x_auth.c_str());
  request.setRawHeader("X-Auth-Signature", x_auth_signature.c_str());
  request.setRawHeader("X-Auth-Nonce", x_auth_nonce.c_str());
  request.setRawHeader("X-Auth-Timestamp", x_auth_timestamp.c_str());
  request.setRawHeader("X-Auth-Version", x_auth_version.c_str());

  auto* client =
    net::http::qhttp_request_client::create_signed(networkmanager_, request, std::move(payload));
  client->post_request(std::move(handler));
}

// ----------------------------------------------------------------------------
void make_request(QNetworkAccessManager& networkmanager)
{
  // std::string url_path = "/api/v2/open_orders/all/";
  // std::string url_path = "/api/v2/balance/";
  std::string url_path = "/api/v2/websockets_token/";
  std::string url_query = "";

  //test1_dbg<0>.debug(str<>("ref count"), client.get(), "test", client.use_count());

  // Run
  account_request(networkmanager, url_path, url_query, [](QByteArray byteArray) {
    std::string_view reply(byteArray.constData(), byteArray.length());
    std::cout << "Response : " << reply << std::endl;
    reply_ready = 1;
    QCoreApplication::exit(0);
  });
}

// ----------------------------------------------------------------------------
// Do not use any Qt objects in another thread before creating the QCoreApplication
// ----------------------------------------------------------------------------
int qt_main(int argc, char* argv[])
{
  QCoreApplication a(argc, argv);
  QNetworkAccessManager networkmanager;
  //
  api_user = execute_os_command("pass bitstamp/user");
  api_key = execute_os_command("pass bitstamp/api_key");
  api_secret = execute_os_command("pass bitstamp/secret");
  if (api_user.empty() || api_key.empty() || api_secret.empty())
  {
    std::cout << "Set ENV vars for API_KEY and API_SEC " << std::endl;
    return EXIT_FAILURE;
  }

  make_request(networkmanager);

  return a.exec();
}

//----------------------------------------------------------------------------
std::string qt_pool_name = "Qt:pool";
//----------------------------------------------------------------------------
int pika_main(int argc, char** argv)
{
  namespace ex = pika::execution::experimental;
  namespace tt = pika::this_thread::experimental;

  // Get a scheduler on the thread pool we have reserved for Qt
  auto qt_sch = ex::thread_pool_scheduler{&pika::resource::get_thread_pool(qt_pool_name)};

  // create a sender to transfer work to the qt pool scheduler
  auto snd = ex::transfer_just(qt_sch) | ex::then([argc, argv]() {
    // run the main qt application entry on our thread
    qt_main(argc, argv);
  });

  // launch and block on completion of the Qt application thread
  tt::sync_wait(std::move(snd));

  // allow pika to shutdown
  pika::finalize();
  return 0;
}

//----------------------------------------------------------------------------
void init_resource_partitioner_handler(
  pika::resource::partitioner& rp, pika::program_options::variables_map const& vm)
{
  // Don't create the pool if the user disabled it
  if (vm["no-qt-pool"].as<bool>())
  {
    qt_pool_name = "default";
    return;
  }

  using pika::threads::scheduler_mode;
#ifdef GROX_DISABLE_IDLE_BACKOFF
  auto mode = scheduler_mode::default_mode;
#else
  auto mode = scheduler_mode::default_mode | scheduler_mode::enable_idle_backoff;
#endif

  // Create a thread pool with a single core for Qt
  rp.create_thread_pool(qt_pool_name, pika::resource::scheduling_policy::unspecified, mode);
  // set the schedule mode for the default pool
  //  rp.create_thread_pool("default", pika::resource::scheduling_policy::shared_priority, mode);
  rp.create_thread_pool("default", pika::resource::scheduling_policy::unspecified, mode);
  rp.add_resource(rp.numa_domains()[0].cores()[0].pus()[0], qt_pool_name);
}

//----------------------------------------------------------------------------
// the normal int main function that is called at startup and runs on an OS
// thread the user must call pika::init to start the pika runtime which
// will execute pika_main on an pika thread
int main(int argc, char* argv[])
{
  namespace po = pika::program_options;

  // Configure application-specific options.
  po::options_description cmdline("usage: grox [options]");

  // clang-format off
  cmdline.add_options()("no-qt-pool",
    pika::program_options::bool_switch(),
    "Disable the Qt pool.");
  // clang-format on

  // Initialize and run pika.
  pika::init_params init_args;
  init_args.desc_cmdline = cmdline;
  // Set the callback to init thread_pools
  init_args.rp_callback = &init_resource_partitioner_handler;
  // tell the scheduler to sleep quickly when there are no tasks to work on
  init_args.cfg = {"pika.max_idle_loop_count=10"};

  auto result = pika::init(pika_main, argc, argv, init_args);
  return result;
}
