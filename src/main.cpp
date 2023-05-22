#include <QApplication>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <memory>
#include <regex>

#include "debug/print.hpp"
#include "mainwindow.hpp"
#include "network/evp-encrypt.hpp"
#include "settings.hpp"
#include "widgets/password_dialog.hpp"
//
#include <pika/init.hpp>
#include <pika/modules/execution.hpp>
#include <pika/modules/executors.hpp>
#include <pika/modules/resource_partitioner.hpp>
#include <pika/modules/schedulers.hpp>
#include <pika/modules/thread_manager.hpp>
#include <pika/program_options.hpp>

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
constexpr int debug_level = 5;
//
template <int Level>
static print_threshold<Level, debug_level> app_dbg("App-Main");

// ----------------------------------------------------------------------------
app_settings* global_settings()
{
  // create a global singleton and return an instance to it
  static std::unique_ptr<app_settings> settings = std::make_unique<app_settings>();
  return settings.get();
}

// ----------------------------------------------------------------------------
void init_settings(app_settings* settings)
{
  settings->tempLocation =
    QStandardPaths::standardLocations(QStandardPaths::TempLocation).first().toLatin1().data();
  settings->configLocation =
    QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first().toLatin1().data();
  settings->appDataLocation =
    QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first().toLatin1().data();
  //
  settings->hdfFileName = settings->appDataLocation + "/grox.hdf5";
  settings->logFileName = QLatin1String("grox.log").data();
  settings->iniFileName = (settings->configLocation + QLatin1String("/grox.ini")).toLatin1().data();
  app_dbg<5>.debug(str<>("Ini"), settings->iniFileName.toLatin1().data());
}

QByteArray base64_encode(const QByteArray& ba)
{
  return ba.toBase64();
}

QByteArray base64_encode(const secure_string& s)
{
  QByteArray ba(s.data(), s.size());
  return ba.toBase64();
}

QByteArray base64_decode(QByteArray ba)
{
  return QByteArray::fromBase64(ba);
}

QByteArray base64_decode(const secure_string& s)
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
  app_settings* app_ini = global_settings();
  QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
  //
  app_ini->grox_password = npw.getPassword().toStdString();

  // we write a dummy random number to ini file
  // if this is present assume that the initial encryption step is valid
  secure_string adummy_string = generate_random_alphanumeric_string(encryption::BLOCK_SIZE, 111111);
  settings.setValue(
    "EncodedData/randomBytes", QString::fromStdString(base64_encode(adummy_string).toStdString()));

  // -----------------------
  // Generate encrypted data
  // -----------------------
  encryption encryptor(app_ini->grox_password, app_ini->randomBytes);
  //
  auto& bitstamp = bitstamp_network::get_bitstamp_instance()->account();
  bitstamp.API_user = npw.getAPIUser().toStdString();
  bitstamp.API_key = npw.getAPIKey().toStdString();
  bitstamp.API_secret = npw.getAPISecret().toStdString();
  bitstamp.tag_ = npw.getAPIDestTag().toLong();
  bitstamp.public_ = npw.getAPIXRPAddress().toStdString();
  secure_string API_user = encryptor.encrypt(bitstamp.API_user);
  secure_string API_key = encryptor.encrypt(bitstamp.API_key);
  secure_string API_secret = encryptor.encrypt(bitstamp.API_secret);
  secure_string API_tag_ = encryptor.encrypt(std::to_string(bitstamp.tag_));
  secure_string API_public_ = encryptor.encrypt(bitstamp.public_);
  //
  settings.setValue(
    "EncryptedData/API_key", QString::fromStdString(base64_encode(API_key).toStdString()));
  settings.setValue(
    "EncryptedData/API_user", QString::fromStdString(base64_encode(API_user).toStdString()));
  settings.setValue(
    "EncryptedData/API_secret", QString::fromStdString(base64_encode(API_secret).toStdString()));
  settings.setValue(
    "EncryptedData/API_desttag", QString::fromStdString(base64_encode(API_tag_).toStdString()));
  settings.setValue("EncryptedData/API_xrpaddress",
    QString::fromStdString(base64_encode(API_public_).toStdString()));
  //
  xrpl_network::get_xrpl_instance(true)->clear_wallets();
  xrpl_network::get_xrpl_instance(false)->clear_wallets();
  int index = 0;
  for (const auto& w : npw.get_wallets())
  {
    std::dynamic_pointer_cast<xrpl_network>(w.network_)->add_wallet(w);
    secure_string name_ = encryptor.encrypt(w.name_);
    secure_string public_ = encryptor.encrypt(w.public_);
    secure_string private_ = encryptor.encrypt(w.private_);
    QString num = QString::number(index++);
    //
    settings.setValue(
      "EncryptedData/XRP_name_" + num, QString::fromStdString(base64_encode(name_).toStdString()));
    settings.setValue("EncryptedData/XRP_public_" + num,
      QString::fromStdString(base64_encode(public_).toStdString()));
    settings.setValue("EncryptedData/XRP_secret_" + num,
      QString::fromStdString(base64_encode(private_).toStdString()));
    xrpl_network* net = dynamic_cast<xrpl_network*>(w.network_.get());
    settings.setValue("EncryptedData/XRP_test_" + num, net->testnet());
  }
}

// ----------------------------------------------------------------------------
std::string exec(const char* cmd)
{
  std::array<char, 1024> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
  {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
  {
    result += buffer.data();
  }
  return result;
}

// ----------------------------------------------------------------------------
int qt_main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QIcon icon(":images/xrp.ico");
  app.setWindowIcon(icon);
  app.setApplicationName("grox");

  // disable stdout buffering so that messages appear right away
  // (especially noticable in debugger terminal)
  app_dbg<0>.eval([]() { std::cout.setf(std::ios::unitbuf); });

  init_settings(global_settings());
  //
  app_settings* app_ini = global_settings();
  QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
  //
  bool authenticated = false;
  if (std::getenv("rand3") != nullptr)
  {
    // generate a base64 encoded pw : bash commmand : echo "password" | base64
    std::string raw = std::getenv("rand3");
    app_ini->grox_password = base64_decode(raw).toStdString();
    app_ini->grox_password = app_ini->grox_password.substr(8, 13);
    authenticated = true;
    app_dbg<5>.debug(str<>("authentication"), "getenv", "ok");
  }
  if (!authenticated)
  {
    std::string commandLine = "timeout 5 ssh pi@192.168.1.15 cat /home/pi/.ssh/.skey.sh";
    auto result = exec(commandLine.c_str());
    std::regex rgx(".*rand3=\"(.*)\".*");
    std::smatch match;
    if (std::regex_search(result, match, rgx))
    {
      app_ini->grox_password = base64_decode(match[1]).toStdString();
      app_ini->grox_password = app_ini->grox_password.substr(8, 13);
      authenticated = true;
      app_dbg<5>.debug(str<>("authentication"), "pi", "ok");
    }
    else
    {
      app_dbg<5>.error(str<>("Authentication"), "pi", "fail");
    }
  }
  if (!authenticated)
  {
    password_dialog npw(true);
    if (npw.exec() == QDialog::Accepted)
    {
      app_ini->grox_password = npw.getPassword().toStdString();
      authenticated = true;
      app_dbg<5>.debug(str<>("authentication"), "password", "ok");
    }
  }
  if (!authenticated)
  {
    app_dbg<5>.error(str<>("Authentication"), "fail");
    //    return EXIT_FAILURE;
  }

  // we need random data for the encryption block
  app_ini->randomBytes = generate_random_alphanumeric_string(encryption::BLOCK_SIZE, 654792);

  // read random initialization data
  QByteArray rand = base64_decode(settings.value("EncodedData/randomBytes", "").toByteArray());

  // if the randomblock is empty (first time app is run?)
  // then we should ask the user for new password and account details
  if (rand.size() != encryption::BLOCK_SIZE)
  {
    password_dialog npw(false);
    if (npw.exec() == QDialog::Accepted)
    {
      generate_encrypted_ini_data(npw);
    }
  }
  else
  {
    // ---------------------------------------
    // decode and decrypt base64 keys
    // ---------------------------------------
    encryption encryptor(app_ini->grox_password, app_ini->randomBytes);

    // ---------------------------------------
    // Bitstamp exchange details
    // ---------------------------------------
    app_ini->networks_.push_back(bitstamp_network::get_bitstamp_instance());
    auto& bitstamp = bitstamp_network::get_bitstamp_instance()->account();
    bitstamp.network_ = bitstamp_network::get_bitstamp_instance();

    QByteArray API_user = base64_decode(settings.value("EncryptedData/API_user", "").toByteArray());
    bitstamp.API_user = encryptor.decrypt(secure_string(API_user.data(), API_user.size()));
    //
    QByteArray API_key = base64_decode(settings.value("EncryptedData/API_key", "").toByteArray());
    bitstamp.API_key = encryptor.decrypt(secure_string(API_key.data(), API_key.size()));
    if (std::getenv("Rand2"))
    {
      bitstamp.API_key = std::getenv("Rand2");
      app_dbg<5>.debug(str<>("Using ENV key"));
    }
    //
    QByteArray API_secret =
      base64_decode(settings.value("EncryptedData/API_secret", "").toByteArray());
    bitstamp.API_secret = encryptor.decrypt(secure_string(API_secret.data(), API_secret.size()));
    if (std::getenv("Rand3"))
    {
      bitstamp.API_secret = std::getenv("Rand3");
      app_dbg<5>.debug(str<>("Using ENV sec"));
    }
    //
    QByteArray API_tag_ =
      base64_decode(settings.value("EncryptedData/API_desttag", "").toByteArray());
    bitstamp.tag_ =
      std::atol(encryptor.decrypt(secure_string(API_tag_.data(), API_tag_.size())).c_str());

    //
    QByteArray API_public_ =
      base64_decode(settings.value("EncryptedData/API_xrpaddress", "").toByteArray());
    bitstamp.public_ = encryptor.decrypt(secure_string(API_public_.data(), API_public_.size()));

    // ---------------------------------------
    // XRP wallet details
    // ---------------------------------------
    app_ini->networks_.push_back(xrpl_network::get_instance(false));
    app_ini->networks_.push_back(xrpl_network::get_instance(true));
    //
    bool present = true;
    int index = 0;
    while (present)
    {
      QString num = QString::number(index);
      if (!settings.contains("EncryptedData/XRP_name_" + num))
        present = false;
      else
      {
        ledger_wallet w;
        //
        bool XRP_testnet = settings.value("EncryptedData/XRP_test_" + num, "false").toBool();
        if (XRP_testnet)
        {
          w.network_ = xrpl_network::get_xrpl_instance(true);
          w.testnet_ = true;
        }
        else
        {
          w.network_ = xrpl_network::get_xrpl_instance(false);
          w.testnet_ = false;
        }
        w.tag_ = 0;
        w.widget_ = nullptr;
        //
        QByteArray XRP_name =
          base64_decode(settings.value("EncryptedData/XRP_name_" + num, "").toByteArray());
        w.name_ = encryptor.decrypt(secure_string(XRP_name.data(), XRP_name.size()));
        //
        QByteArray XRP_public =
          base64_decode(settings.value("EncryptedData/XRP_public_" + num, "").toByteArray());
        w.public_ = encryptor.decrypt(secure_string(XRP_public.data(), XRP_public.size()));
        //
        QByteArray XRP_secret =
          base64_decode(settings.value("EncryptedData/XRP_secret_" + num, "").toByteArray());
        w.private_ = encryptor.decrypt(secure_string(XRP_secret.data(), XRP_secret.size()));

        std::dynamic_pointer_cast<xrpl_network>(w.network_)->add_wallet(w);
      }
      index++;
    }
  }

  if (argc > 1 && std::string(argv[1]) == std::string("decode"))
  {
    auto& bitstamp = bitstamp_network::get_bitstamp_instance()->account();
    app_dbg<5>.debug("\nDecrypted information\n");
    app_dbg<5>.debug("API_user       : ", bitstamp.API_user);
    app_dbg<5>.debug("API_key        : ", bitstamp.API_key);
    app_dbg<5>.debug("API_secret     : ", bitstamp.API_secret);
    app_dbg<5>.debug("xrp.tag        : ", bitstamp.tag_);
    app_dbg<5>.debug("xrp.public     : ", bitstamp.public_);
    //
    auto const& x1 = xrpl_network::get_xrpl_instance(false)->wallets();
    auto const& x2 = xrpl_network::get_xrpl_instance(true)->wallets();
    for (const auto lw : x1)
    {
      auto w = static_cast<ledger_wallet*>(lw);
      app_dbg<5>.debug("XRP_name       : ", w->name_);
      app_dbg<5>.debug("XRP_public     : ", w->public_);
      app_dbg<5>.debug("XRP_secret     : ", w->private_);
      app_dbg<5>.debug("XRP_testnet    : ", w->testnet_);
    }
    for (const auto lw : x2)
    {
      auto w = static_cast<ledger_wallet*>(lw);
      app_dbg<5>.debug("XRP_name       : ", w->name_);
      app_dbg<5>.debug("XRP_public     : ", w->public_);
      app_dbg<5>.debug("XRP_secret     : ", w->private_);
      app_dbg<5>.debug("XRP_testnet    : ", w->testnet_);
    }
    return EXIT_SUCCESS;
  }

  GroxMainWindow mainWindow;

  QObject::connect(&app, SIGNAL(aboutToQuit()), &mainWindow, SLOT(appExitCleanupHandler()));
  QObject::connect(&mainWindow, SIGNAL(quitApplication()), &app, SLOT(quit()));

  mainWindow.resize(1024, 768);
  mainWindow.show();

  return app.exec();
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
  return pika::finalize();
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
  rp.create_thread_pool("default", pika::resource::scheduling_policy::shared_priority, mode);
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
  po::options_description cmdline("usage: " PIKA_APPLICATION_STRING " [options]");

  // clang-format off
    cmdline.add_options()("no-qt-pool", pika::program_options::bool_switch(),
        "Disable the Qt pool.");

    cmdline.add_options()("decode",
        po::value<std::string>()->default_value(""),
        "shortcut");

    cmdline.add_options()("disable something",
        po::value<bool>()->default_value(false),
        "placeholder for disabling some functionality");
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
