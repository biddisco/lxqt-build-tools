#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
//
#include <exec/async_scope.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <stdexec/execution.hpp>
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
#include "debug/logging.hpp"
#include "exchange/bitstamp.hpp"
#include "senders/pika_stdexec.hpp"
#include "senders/qhttp-post-sender.hpp"
#include "senders/qtstdexec.hpp"
//
namespace ex = stdexec;
namespace tt = pika::this_thread::experimental;
//
std::string qt_pool_name = "Qt:pool";
std::shared_ptr<bitstamp_network> bitstamp;

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
  static auto test1_log = grox::log::create("test-exB");
}    // namespace

// ------------------------------------------------------------------
TEST(abstract_exchange, request_account_info)
{
  // note pika::this_thread::sync_wait yields task, but stdexec::sync_wait blocks thread
  namespace tt = pika::this_thread::experimental;
  //
  GROX_LOG_DEBUG(test1_log, "{:>20} request_account_info", "TEST");
  std::atomic<bool> finished{false};
  bitstamp_account& acct = bitstamp->accounts()[0];
  auto snd = ex::starts_on(QtStdExec::QThreadScheduler(), ex::just())                //
      | ex::let_value([&acct]() { return bitstamp->request_account_info(acct); })    // Qt -> pika
      | ex::then([&acct, &finished](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          nlohmann::json jdata = nlohmann::json::parse(data);
          GROX_LOG_DEBUG(test1_log, "{:>20} {}", "request_account_info", jdata.dump(4));
          EXPECT_TRUE(jdata.size() > 0);
          EXPECT_TRUE(jdata["eur_available"] != "");
          finished = true;
        });

  GROX_LOG_DEBUG(test1_log, "{:>20} request_account_info", "SYNC_WAIT");
  tt::sync_wait(std::move(snd));
  EXPECT_TRUE(finished);
  GROX_LOG_DEBUG(test1_log, "{:>20} request_account_info", "COMPLETE");
}

// ------------------------------------------------------------------
TEST(abstract_exchange, request_all_account_infos)
{
  // note pika::this_thread::sync_wait yields task, but stdexec::sync_wait blocks thread
  namespace tt = pika::this_thread::experimental;
  //
  GROX_LOG_DEBUG(test1_log, "{:>20} request_all_account_infos", "TEST");
  std::atomic<std::size_t> finished{bitstamp->wallets().size()};

  auto get_all_account_infos = [&finished]() {
    exec::async_scope scope;
    for (auto& acct : bitstamp->accounts())
    {
      auto handle_account_info = [&acct, &finished](QByteArray byteArray) {
        std::string_view data(byteArray.constData(), byteArray.length());
        nlohmann::json jdata = nlohmann::json::parse(data);
        GROX_LOG_DEBUG(test1_log, "{:>20} {} {}", "handle_account_info", acct.name_, jdata.dump(4));
        EXPECT_TRUE(jdata.size() > 0);
        EXPECT_TRUE(jdata["eur_available"] != "");
        finished--;
        GROX_LOG_DEBUG(
            test1_log, "{:>20} complete {} {}", "handle_account_info", acct.name_, finished.load());
      };

      auto snd = ex::starts_on(QtStdExec::QThreadScheduler(), ex::just())                // Qt
          | ex::let_value([&acct]() { return bitstamp->request_account_info(acct); })    // -> pika
          | ex::then(handle_account_info);

      scope.spawn(std::move(snd));
    }

    GROX_LOG_DEBUG(test1_log, "{:>20} request_all_account_infos", "SYNC_WAIT");
    EXPECT_TRUE(pika::this_thread::get_pool()->get_pool_name() != qt_pool_name);
    tt::sync_wait(scope.on_empty());
    GROX_LOG_DEBUG(test1_log, "{:>20} request_all_account_infos", "COMPLETE");
  };

  stdexec::sender auto snd =
      stdexec::starts_on(grox::senders::default_pool_scheduler(), stdexec::just())    //
      | stdexec::then(get_all_account_infos);

  GROX_LOG_DEBUG(test1_log, "{:>20} scope", "SYNC_WAIT");
  EXPECT_TRUE(pika::this_thread::get_pool()->get_pool_name() != qt_pool_name);
  tt::sync_wait(std::move(snd));
  GROX_LOG_DEBUG(test1_log, "{:>20} scope", "COMPLETE");
}

// ----------------------------------------------------------------------------
TEST(abstract_exchange, cancel_order)
{
  trade_data t;
  t.id_ = 1816315536502784;
  t.wallet_ = "Main";

  GROX_LOG_DEBUG(test1_log, "{:>20}", "TEST(abstract_exchange, request_account_info)");
  std::atomic<bool> finished{false};
  auto snd = ex::starts_on(QtStdExec::QThreadScheduler(), ex::just())         //
      | ex::let_value([t]() { return bitstamp->request_cancel_order(t); })    // Qt -> pika
      | ex::then([&](QByteArray byteArray) {
          std::string_view data(byteArray.constData(), byteArray.length());
          nlohmann::json jdata = nlohmann::json::parse(data);
          GROX_LOG_DEBUG(test1_log, "{:>20} {}", "cancel_order", jdata.dump(4));
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
  GROX_LOG_DEBUG(test1_log, "{:>20}", "enter qt_main");
  QCoreApplication a(argc, argv);
  // the lifetime of the network manager must be as long as the qt application
  QNetworkAccessManager networkmanager;
  global_settings.networkmanager_ = &networkmanager;
  //
  bitstamp = std::make_shared<bitstamp_network>();
  for (auto& acct : bitstamp->accounts())
  {
    if (!bitstamp_network::get_pass_authentication(acct))
    {
      std::cout << "Password authentication failed" << std::endl;
      return EXIT_FAILURE;
    }
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
  bitstamp.reset();
  return test_result;
}

//----------------------------------------------------------------------------
// pika_main - executes on an pika thread
int pika_main(int argc, char** argv, pika::program_options::variables_map& vm)
{
  GROX_LOG_DEBUG(test1_log, "{:>20}", "enter pika_main");
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
    GROX_LOG_DEBUG(test1_log, "{:>20}", "qt_main complete");
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
