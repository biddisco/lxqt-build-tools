#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
//
#include <range/v3/view.hpp>
#include <fmt/format.h>
//
#include <QApplication>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>
//
#include <pika/init.hpp>
#include <pika/modules/execution.hpp>
#include <pika/modules/executors.hpp>
#include <pika/modules/resource_partitioner.hpp>
#include <pika/modules/schedulers.hpp>
#include <pika/modules/thread_manager.hpp>
#include <pika/program_options.hpp>
//
#include "indicators/indicator_registry.hpp"
#include "indicators/python_plugin.hpp"
#include "network/evp-encrypt.hpp"
#include "util/execute_os_command.hpp"
#include "util/stringutils.hpp"
#include "widgets/password_dialog.hpp"
//
#include "debug/logging.hpp"
#include "mainwindow.hpp"

// ----------------------------------------------------------------------------
static auto app_log = grox::log::create("App-Main");

// save these to pass to Qt init.
static int argc;
static char** argv;

// ----------------------------------------------------------------------------
void init_settings(app_settings* settings, QNetworkAccessManager* networkmanager)
{
  settings->networkmanager_ = networkmanager;
  settings->tempLocation =
      QStandardPaths::standardLocations(QStandardPaths::TempLocation).first().toStdString();
  settings->configLocation =
      QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first().toStdString();
  settings->appDataLocation =
      QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first().toStdString();
  //
  settings->hdfFileName = "grox.hdf5";
  settings->logFileName = QLatin1String("grox.log").data();
  settings->iniFileName = to_qstring(fmt::format("{}/grox.ini", settings->configLocation));
  GROX_LOG_DEBUG(app_log, "{:>20} {}", "Ini", settings->iniFileName.toStdString());
}

QByteArray base64_encode(QByteArray const& ba) { return ba.toBase64(); }

QByteArray base64_encode(secure_string const& s)
{
  QByteArray ba(s.data(), s.size());
  return ba.toBase64();
}

QByteArray base64_decode(QByteArray ba) { return QByteArray::fromBase64(ba); }

QByteArray base64_decode(secure_string const& s)
{
  return QByteArray::fromBase64(QByteArray::fromStdString(s));
}

secure_string base64_string(QByteArray ba)
{
  QByteArray bb = QByteArray::fromBase64(ba);
  return secure_string(bb.data(), bb.size());
}

// ----------------------------------------------------------------------------
void generate_encrypted_ini_data(password_dialog& npw)
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  // ----------------------------------------------
  // Generate data we need for encryption
  // ----------------------------------------------
  global_settings.grox_password = npw.getPassword().toStdString();

  // we write a dummy random number to ini file
  // if this is present assume that the initial encryption step is valid
  secure_string adummy_string = generate_random_alphanumeric_string(encryption::BLOCK_SIZE, 111111);
  settings.setValue(
      "EncodedData/randomBytes", to_qstring(base64_encode(adummy_string).toStdString()));

  // create an encryption helper object
  encryption encryptor(global_settings.grox_password, global_settings.randomBytes);

  // -------------------------------------
  // Write Bitstamp accounts
  settings.beginGroup("Bitstamp");
  auto bitstamp = bitstamp_network::get_bitstamp_instance();
  for (auto const& [i, acct] : bitstamp->accounts() | ranges::views::enumerate)
  {
    if (!bitstamp_network::get_pass_authentication(acct))
    {
      throw std::runtime_error("Password authentication failed");
    }
    settings.beginGroup(acct.name_);
    acct.tag_ = npw.getBitstampDestTag(i).toLong();
    acct.public_ = npw.getBitstampXRPAddress(i).toStdString();

    // api key/secret and user are taken  from pass authentication and not written to ini
    // secure_string API_user = encryptor.encrypt(acct.API_user);
    // secure_string API_key = encryptor.encrypt(acct.API_key);
    // secure_string API_secret = encryptor.encrypt(acct.API_secret);
    // settings.setValue("API_key", to_qstring(base64_encode(API_key).toStdString()));
    // settings.setValue("API_user", to_qstring(base64_encode(API_user).toStdString()));
    // settings.setValue("API_secret", to_qstring(base64_encode(API_secret).toStdString()));

    secure_string API_tag_ = encryptor.encrypt(std::to_string(acct.tag_));
    secure_string API_public_ = encryptor.encrypt(acct.public_);
    settings.setValue("API_desttag", to_qstring(base64_encode(API_tag_).toStdString()));
    settings.setValue("API_xrpaddress", to_qstring(base64_encode(API_public_).toStdString()));
    settings.endGroup();    // account
  }
  settings.endGroup();    // bitstamp

  // -------------------------------------
  // Write XRPL accounts
  // loop over wallets twice, once for main-net once for test-net
  auto xrp_wallets = npw.getXRPwallets();
  for (auto const is_test : {false, true})
  {
    settings.beginGroup(is_test ? "XRPL-testnet" : "XRPL-mainnet");
    xrpl_network::get_xrpl_instance(is_test)->clear_wallets();
    std::vector<ledger_wallet> wallets2;
    std::copy_if(
        xrp_wallets.begin(), xrp_wallets.end(), std::back_inserter(wallets2), [is_test](auto& w) {
          return is_test == std::dynamic_pointer_cast<xrpl_network>(w.network_)->testnet();
        });
    for (auto const& w : wallets2)
    {
      settings.beginGroup(w.name_);
      std::dynamic_pointer_cast<xrpl_network>(w.network_)->add_wallet(w);
      secure_string public_ = encryptor.encrypt(w.public_);
      secure_string private_ = encryptor.encrypt(w.private_);
      //
      settings.setValue("public", to_qstring(base64_encode(public_).toStdString()));
      settings.setValue("secret", to_qstring(base64_encode(private_).toStdString()));
      settings.endGroup();    // wallet
    }
    settings.endGroup();    // main/test network
  }
}

// ----------------------------------------------------------------------------
int qt_main(pika::program_options::variables_map& vm)
{
  // make sure networkmanager is created on *<this thread>*
  QApplication app(argc, argv);
  QNetworkAccessManager networkmanager;

  // setup resources that Qt uses for pics etc
  Q_INIT_RESOURCE(images);
  QIcon icon(":images/icons/xrp-logo-white-black.svg");
  app.setWindowIcon(icon);
  app.setApplicationName("grox");

  // disable stdout buffering so that messages appear right away
  // (especially noticable in debugger terminal)
  std::cout.setf(std::ios::unitbuf);

  // initialize global settings : @TODO - get rid of this singleton?
  init_settings(&global_settings, &networkmanager);

  // open the Qt application ini file and start reading state
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  //
  bool authenticated = false;
  std::string commandLine = "pass grox/grox";
  auto result = execute_os_command(commandLine.c_str());
  if (result.size() > 0)
  {
    global_settings.grox_password = result;
    authenticated = true;
    GROX_LOG_DEBUG(app_log, "{:>20} {} {}", "authentication", "pass", "ok");
  }
  else { GROX_LOG_ERROR(app_log, "{:>20} {} {}", "Authentication", "pass", "fail"); }

  // if pass command failed, then allow user to enter password via dialog box
  if (!authenticated)
  {
    password_dialog npw(true);
    if (npw.exec() == QDialog::Accepted)
    {
      global_settings.grox_password = npw.getPassword().toStdString();
      authenticated = true;
      GROX_LOG_DEBUG(app_log, "{:>20} {} {}", "authentication", "password", "ok");
    }
  }
  if (!authenticated)
  {
    GROX_LOG_ERROR(app_log, "{:>20} {} {}", "Authentication", "fail",
        "No access to accounts/wallets available");
  }

  // we need random data for the encryption block
  global_settings.randomBytes = generate_random_alphanumeric_string(encryption::BLOCK_SIZE, 654792);

  // read random initialization data
  QByteArray rand = base64_decode(settings.value("EncodedData/randomBytes", "").toByteArray());

  // if the randomblock is empty (first time app is run?)
  // then we should ask the user for new password and account details
  if (rand.size() != encryption::BLOCK_SIZE)
  {
    password_dialog npw(false);
    if (npw.exec() == QDialog::Accepted) { generate_encrypted_ini_data(npw); }
  }
  else
  {
    // ---------------------------------------
    // decode and decrypt base64 keys
    // ---------------------------------------
    encryption encryptor(global_settings.grox_password, global_settings.randomBytes);

    // ---------------------------------------
    // Bitstamp abstract_exchange details
    // ---------------------------------------
    global_settings.networks_.push_back(bitstamp_network::get_bitstamp_instance());
    auto bitstamp = bitstamp_network::get_bitstamp_instance();
    settings.beginGroup("Bitstamp");
    for (auto& acct : bitstamp->accounts())
    {
      if (!bitstamp_network::get_pass_authentication(acct))
      {
        throw std::runtime_error("Password authentication failed");
      }
      acct.network_ = bitstamp;
      //
      settings.beginGroup(acct.name_);
      QByteArray API_tag_ = base64_decode(settings.value("API_desttag").toByteArray());
      acct.tag_ =
          std::atol(encryptor.decrypt(secure_string(API_tag_.data(), API_tag_.size())).c_str());
      //
      QByteArray API_public_ = base64_decode(settings.value("API_xrpaddress").toByteArray());
      acct.public_ = encryptor.decrypt(secure_string(API_public_.data(), API_public_.size()));
      settings.endGroup();
    }
    settings.endGroup();

    // ---------------------------------------
    // XRP wallet details
    // ---------------------------------------
    global_settings.networks_.push_back(xrpl_network::get_instance(false));
    global_settings.networks_.push_back(xrpl_network::get_instance(true));

    // -------------------------------------
    for (auto const is_test : {false, true})
    {
      auto xrpl = xrpl_network::get_xrpl_instance(is_test);
      settings.beginGroup(is_test ? "XRPL-testnet" : "XRPL-mainnet");
      // get all wallets
      QStringList children = settings.childGroups();
      for (auto const& wallet : children)
      {
        settings.beginGroup(wallet);
        ledger_wallet w;
        w.name_ = wallet.toStdString();
        w.testnet_ = is_test;
        //
        QByteArray XRP_public = base64_decode(settings.value("public").toByteArray());
        w.public_ = encryptor.decrypt(secure_string(XRP_public.data(), XRP_public.size()));
        //
        QByteArray XRP_secret = base64_decode(settings.value("secret").toByteArray());
        w.private_ = encryptor.decrypt(secure_string(XRP_secret.data(), XRP_secret.size()));
        //
        w.network_ = xrpl;
        xrpl->add_wallet(w);
        settings.endGroup();    // wallet
      }
      settings.endGroup();    // main/test network
    }
  }

#define GROX_SUPPORT_DECODE 1
#ifdef GROX_SUPPORT_DECODE
  if (vm["decode"].as<bool>())
  {
    auto& bitstamp = bitstamp_network::get_bitstamp_instance()->accounts()[0];
    GROX_LOG_DEBUG(app_log, "\nDecrypted information\n");
    GROX_LOG_DEBUG(app_log, "API_user       : {}", bitstamp.API_user);
    GROX_LOG_DEBUG(app_log, "API_key        : {}", bitstamp.API_key);
    GROX_LOG_DEBUG(app_log, "API_secret     : {}", bitstamp.API_secret);
    GROX_LOG_DEBUG(app_log, "xrp.tag        : {}", bitstamp.tag_);
    GROX_LOG_DEBUG(app_log, "xrp.public     : {}", bitstamp.public_);
    //
    auto const& x1 = xrpl_network::get_xrpl_instance(false)->wallets();
    auto const& x2 = xrpl_network::get_xrpl_instance(true)->wallets();
    for (auto const lw : x1)
    {
      auto w = static_cast<ledger_wallet*>(lw);
      GROX_LOG_DEBUG(app_log, "XRP_name       : {}", w->name_);
      GROX_LOG_DEBUG(app_log, "XRP_public     : {}", w->public_);
      GROX_LOG_DEBUG(app_log, "XRP_secret     : {}", w->private_);
      GROX_LOG_DEBUG(app_log, "XRP_testnet    : {}", w->testnet_);
    }
    for (auto const lw : x2)
    {
      auto w = static_cast<ledger_wallet*>(lw);
      GROX_LOG_DEBUG(app_log, "XRP_name       : {}", w->name_);
      GROX_LOG_DEBUG(app_log, "XRP_public     : {}", w->public_);
      GROX_LOG_DEBUG(app_log, "XRP_secret     : {}", w->private_);
      GROX_LOG_DEBUG(app_log, "XRP_testnet    : {}", w->testnet_);
    }
    return EXIT_SUCCESS;
  }
#endif

  // ------------------------------------------------------------------------
  // Load indicator plugins from standard plugin directory
  // ------------------------------------------------------------------------
  GROX_LOG_DEBUG(app_log, "Loading indicator plugins...");
  auto& registry = indicators::indicator_registry::getInstance();

  // Try multiple possible plugin locations:
  // 1. Current build directory (development builds)
  // 2. Relative to build/bin
  // 3. Standard install location
  std::vector<std::string> plugin_dirs = {
      "./lib/grox/plugins",             // Build directory root
      "../lib/grox/plugins",            // From build/bin directory
      "./plugins",                      // Same directory as executable
      "/usr/local/lib/grox/plugins",    // Standard install location
  };

  std::size_t plugins_loaded = 0;
  for (auto const& dir : plugin_dirs)
  {
    GROX_LOG_DEBUG(app_log, "Trying plugin directory: {}", dir);
    plugins_loaded += registry.load_plugins_from_directory(dir);
  }

  GROX_LOG_DEBUG(app_log, "Loaded {} indicator plugin(s)", plugins_loaded);

  // ------------------------------------------------------------------------
  // Initialize and probe Python indicator modules
  // ------------------------------------------------------------------------
  auto& py_registry = indicators::python::python_indicator_registry::instance();
  if (py_registry.initialize())
  {
    std::size_t py_modules_loaded = 0;
    for (auto const& dir : plugin_dirs)
    {
      auto const py_dir = fmt::format("{}/python", dir);
      GROX_LOG_DEBUG(app_log, "Trying python plugin directory: {}", py_dir);
      py_modules_loaded += py_registry.load_indicators_from_directory(py_dir);
    }

    auto const available_python_indicators = py_registry.get_available_indicators();
    GROX_LOG_DEBUG(app_log,
        "Loaded {} python indicator module(s); {} python indicator class(es) available",
        py_modules_loaded, available_python_indicators.size());

    // Register Python indicators with main registry so they appear in GUI
    std::size_t py_registered = py_registry.register_with_main_registry(registry);
    GROX_LOG_DEBUG(app_log, "Registered {} python indicator(s) with main registry", py_registered);
  }
  else { GROX_LOG_DEBUG(app_log, "Python indicator registry initialization failed"); }

  // ------------------------------------------------------------------------
  // Create main window
  // ------------------------------------------------------------------------
  GroxMainWindow mainWindow;

  // QObject::connect(&app, SIGNAL(aboutToQuit()), &mainWindow, SLOT(appExitCleanupHandler()));
  QObject::connect(&mainWindow, SIGNAL(quitApplication()), &app, SLOT(quit()));

  mainWindow.resize(1024, 768);
  mainWindow.show();

  return app.exec();
}

//----------------------------------------------------------------------------
std::string qt_pool_name = "Qt:pool";

//----------------------------------------------------------------------------
int pika_main(pika::program_options::variables_map& vm)
{
  grox::log::init_from_env();

  namespace ex = pika::execution::experimental;
  namespace tt = pika::this_thread::experimental;

  // Get a scheduler on the thread pool we have reserved for Qt
  auto qt_sch = ex::thread_pool_scheduler{&pika::resource::get_thread_pool(qt_pool_name)};

  // create a sender to transfer work to the qt pool scheduler
  auto snd = ex::transfer_just(qt_sch) | ex::then([&vm]() {
    // run the main qt application entry on our thread
    qt_main(vm);
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
  rp.add_resource(rp.sockets()[0].cores()[0].pus()[0], qt_pool_name);
}

//----------------------------------------------------------------------------
// the normal int main function that is called at startup and runs on an OS
// thread the user must call pika::init to start the pika runtime which
// will execute pika_main on an pika thread

int main(int argc, char* argv[])
{
  // fix: Qt depends on a UTF-8 locale, and has switched to "C.UTF-8" instead
  // TODO: Find a real fix
  setenv("LC_ALL", "C.UTF-8", 1);

  // fix: Qt: Session management error: Could not open network socket
  // TODO: Find a real fix
  unsetenv("SESSION_MANAGER");
  //
  ::argc = argc;
  ::argv = argv;

  namespace po = pika::program_options;

  // Configure application-specific options.
  po::options_description cmdline("usage: grox [options]");

  // clang-format off
  cmdline.add_options()("no-qt-pool",
    pika::program_options::bool_switch(),
    "Disable the Qt pool.");

  cmdline.add_options()("decode",
    pika::program_options::bool_switch(),
    "shortcut");

  cmdline.add_options()("disable something",
    po::value<bool>()->default_value(false),
    "placeholder for disabling some functionality");

  // po::variables_map vm;
  // po::store(po::command_line_parser(argc, argv)
  //                                .allow_unregistered()
  //                                .options(cmdline)
  //                                .run(), vm);
  // clang-format on

  // Initialize and run pika.
  pika::init_params init_args;
  init_args.desc_cmdline = cmdline;
  // Set the callback to init thread_pools
  init_args.rp_callback = &init_resource_partitioner_handler;
  // tell the scheduler to sleep quickly when there are no tasks to work on
  init_args.cfg = {"pika.max_idle_loop_count=10", "pika.os_threads=4"};

  auto result = pika::init(pika_main, argc, argv, init_args);
  return result;
}
