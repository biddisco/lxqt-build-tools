#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
//
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
//
#include <pika/init.hpp>
#include <pika/modules/execution.hpp>
#include <pika/modules/executors.hpp>
#include <pika/modules/resource_partitioner.hpp>
#include <pika/modules/schedulers.hpp>
#include <pika/modules/thread_manager.hpp>
#include <pika/program_options.hpp>
//
#include "config/config.hpp"
#include "debug/print.hpp"
#include "exchange/bitstamp.hpp"
#include "senders/qhttp-post-sender.hpp"
#include "senders/qtstdexec.hpp"
//
namespace ex = stdexec;
namespace tt = pika::this_thread::experimental;
//
std::string qt_pool_name = "Qt:pool";
std::shared_ptr<bitstamp_network> bitstamp_exchange;

/// This test starts up in 'int main' and initializes pika with N threads,
/// it sets the initialization callback for the resource manager to allocate 1
/// thread to a Qt pool which will be reserved for the Qt application thread.
/// pika_main starts the Qt application on the Qt pool thread and then sits and
/// waits for Qt to terminate (when the Qt application is done).
/// Tests are spawned on the pika default pool, and each test, blocks waiting
/// for it to finishe before the next test rruns, when all have finished
/// we terminate the Qt application and qt cleans up, then pika cleans up,
/// then the application terminates

// ----------------------------------------------------------------------------
namespace {
  template <int Level>
  inline constexpr grox::debug::detail::print_threshold<Level, 0> test1_dbg("test-exB");
}    // namespace

// ------------------------------------------------------------------
TEST(exchange, request_account_info)
{
  using namespace grox::debug;
  test1_dbg<2>.debug(ffmt<s20>("TEST(exchange, request_account_info)"));
  std::atomic<bool> finished{false};
  auto wallets = bitstamp_exchange->wallets();
  bitstamp_account& acct = *static_cast<bitstamp_account*>(wallets[0]);
  auto snd = ex::starts_on(QtStdExec::QThreadScheduler(), ex::just())    //
      | ex::let_value(
            [&acct]() { return bitstamp_exchange->request_account_info(acct); })    // Qt -> pika
      | ex::then([&](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          nlohmann::json jdata = nlohmann::json::parse(data);
          test1_dbg<0>.debug(ffmt<s20>("request_account_info"), jdata.dump(4));
          EXPECT_TRUE(jdata.size() > 0);
          EXPECT_TRUE(jdata["eur_available"] != "");
          finished = true;
        });
  ex::start_detached(std::move(snd));
  pika::util::yield_while([&]() { return !finished; });
}

// ----------------------------------------------------------------------------
TEST(exchange, cancel_order)
{
  trade_data t;
  t.id_ = 1816315536502784;

  using namespace grox::debug;
  test1_dbg<2>.debug(ffmt<s20>("TEST(exchange, request_account_info)"));
  std::atomic<bool> finished{false};
  auto snd = ex::starts_on(QtStdExec::QThreadScheduler(), ex::just())                  //
      | ex::let_value([t]() { return bitstamp_exchange->request_cancel_order(t); })    // Qt -> pika
      | ex::then([&](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          nlohmann::json jdata = nlohmann::json::parse(data);
          test1_dbg<0>.debug(ffmt<s20>("cancel_order"), jdata.dump(4));
          EXPECT_EQ(jdata["error"], "Order not found");
          EXPECT_TRUE(jdata.size() == 2);
          finished = true;
        });
  ex::start_detached(std::move(snd));
  pika::util::yield_while([&]() { return !finished; });
}

// ----------------------------------------------------------------------------
// qt-main - runs on a dedicated pika thread in its qt pool
// Do not use any Qt objects in another thread before creating the QCoreApplication
// ----------------------------------------------------------------------------
int qt_main(int argc, char* argv[])
{
  using namespace grox::debug;
  test1_dbg<2>.debug(ffmt<s20>("enter qt_main"));
  QCoreApplication a(argc, argv);
  // the lifetime of the network manager must be as long as the qt application
  QNetworkAccessManager networkmanager;
  global_settings.networkmanager_ = &networkmanager;
  //
  bitstamp_exchange = std::make_shared<bitstamp_network>();
  auto wallets = bitstamp_exchange->wallets();
  bitstamp_account* acct = static_cast<bitstamp_account*>(wallets[0]);
  if (!bitstamp_network::get_pass_authentication(*acct))
  {
    std::cout << "Password authentication failed" << std::endl;
    return EXIT_FAILURE;
  }

  int test_result;
  auto snd = ex::starts_on(grox::senders::default_pool_scheduler(), ex::just())    //
      | ex::then([&test_result]() {
          test_result = RUN_ALL_TESTS();
          QCoreApplication::instance()->quit();
        });
  ex::start_detached(std::move(snd));

  // start the Qt messaging/processing loop, returns only when exits
  a.exec();

  // cleanup bitstamp instance before Qt Application/threads go out of scope
  bitstamp_exchange.reset();
  return test_result;
}

//----------------------------------------------------------------------------
// pika_main - executes on an pika thread
int pika_main(int argc, char** argv, pika::program_options::variables_map& vm)
{
  using namespace grox::debug;
  test1_dbg<2>.debug(ffmt<s20>("enter pika_main"));
  // Get a scheduler on the thread pool we have reserved for Qt
  auto qt_sch = pika::execution::experimental::thread_pool_scheduler{
      &pika::resource::get_thread_pool(qt_pool_name)};

  // create a sender to transfer work to the qt pool scheduler
  auto snd = ex::transfer_just(qt_sch) | ex::then([argc, argv]() {
    // run the main qt application entry on our thread
    return qt_main(argc, argv);
  });

  std::printf("Running pika_main() /*from gtest_pika_main.cpp*/\n");
  auto ret = [&] {
    // launch and block on completion of the Qt application thread
    int test_results = tt::sync_wait(std::move(snd));
    test1_dbg<2>.debug(ffmt<s20>("qt_main complete"));
    return test_results;
  }();
  pika::finalize();
  return ret;
}

//----------------------------------------------------------------------------
void init_qt_pool(pika::resource::partitioner& rp, pika::program_options::variables_map const& vm)
{
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
  rp.add_resource(rp.sockets()[0].cores()[0].pus()[0], qt_pool_name);
}

//----------------------------------------------------------------------------
// the normal int main function that is called at startup and runs on an OS
// thread the user must call pika::init to start the pika runtime which
// will execute pika_main on an pika thread
GTEST_API_ int main(int argc, char** argv)
{
  // required for gtest
  testing::InitGoogleTest(&argc, argv);

  // setup pika initialization including thread pool for Qt main thread
  // tell the scheduler to sleep quickly when there are no tasks to work on
  // if not specified on the command line, ask for 2 threads
  pika::init_params init_args;
  init_args.rp_callback = &init_qt_pool;
  init_args.cfg = {"pika.max_idle_loop_count=10", "pika.os_threads=2"};
  return pika::init(std::bind_front(pika_main, argc, argv), argc, argv, init_args);
}
