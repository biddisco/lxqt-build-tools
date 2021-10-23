#include <QApplication>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <memory>

#include "src/network/evp-encrypt.hpp"
#include "mainwindow.hpp"
#include "src/widgets/password_dialog.hpp"
#include "src/settings.hpp"

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
        QStandardPaths::standardLocations(QStandardPaths::TempLocation)
            .first()
            .toLatin1()
            .data();
    settings->configLocation =
        QStandardPaths::standardLocations(QStandardPaths::ConfigLocation)
            .first()
            .toLatin1()
            .data();
    settings->appDataLocation =
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)
            .first()
            .toLatin1()
            .data();
    //
    settings->hdfFileName = settings->appDataLocation + "/grox.hdf5";
    settings->logFileName = QLatin1String("grox.log").data();
    settings->iniFileName =
        (settings->configLocation + QLatin1String("/grox.ini")).toLatin1().data();
    std::cout << "Ini: " << settings->iniFileName.toLatin1().data() << std::endl;
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

QByteArray base64_decode(const secure_string &s)
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
    settings.setValue("EncodedData/randomBytes",
        QString::fromStdString(base64_encode(adummy_string).toStdString()));

    // -----------------------
    // Generate encrypted data
    // -----------------------
    encryption encryptor(app_ini->grox_password, app_ini->randomBytes);
    //
    auto &bitstamp = bitstamp_network::get_bitstamp_instance()->account();
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
    settings.setValue("EncryptedData/API_key",
        QString::fromStdString(base64_encode(API_key).toStdString()));
    settings.setValue("EncryptedData/API_user",
        QString::fromStdString(base64_encode(API_user).toStdString()));
    settings.setValue("EncryptedData/API_secret",
        QString::fromStdString(base64_encode(API_secret).toStdString()));
    settings.setValue("EncryptedData/API_desttag",
        QString::fromStdString(base64_encode(API_tag_).toStdString()));
    settings.setValue("EncryptedData/API_xrpaddress",
        QString::fromStdString(base64_encode(API_public_).toStdString()));
    //
    xrpl_network::get_xrpl_instance(true)->clear_wallets();
    xrpl_network::get_xrpl_instance(false)->clear_wallets();
    int index = 0;
    for (const auto &w : npw.get_wallets()) {
        std::dynamic_pointer_cast<xrpl_network>(w.network_)->add_wallet(w);
        secure_string name_ = encryptor.encrypt(w.name_);
        secure_string public_ = encryptor.encrypt(w.public_);
        secure_string private_ = encryptor.encrypt(w.private_);
        QString num = QString::number(index++);
        //
        settings.setValue("EncryptedData/XRP_name_" + num,
            QString::fromStdString(base64_encode(name_).toStdString()));
        settings.setValue("EncryptedData/XRP_public_" + num,
            QString::fromStdString(base64_encode(public_).toStdString()));
        settings.setValue("EncryptedData/XRP_secret_" + num,
            QString::fromStdString(base64_encode(private_).toStdString()));
        xrpl_network * net = dynamic_cast<xrpl_network*>(w.network_.get());
        settings.setValue("EncryptedData/XRP_test_" + num, net->testnet());
    }
}

// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QIcon icon(":images/xrp.ico");
    app.setWindowIcon(icon);
    app.setApplicationName("grox");
    //
    init_settings(global_settings());
    //
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    //
    if (std::getenv("rand3") != nullptr)
    {
        // generate a base64 encoded pw : bash commmand : echo "password" | base64
        std::string raw = std::getenv("rand3");
        app_ini->grox_password = base64_decode(raw).toStdString();
        app_ini->grox_password = app_ini->grox_password.substr(8,13);
    }
    else
    {
        std::cout << "Please set GROX_PASSWORD base64 encoded environment var" << std::endl;
        return EXIT_FAILURE;
    }

    // we need random data for the encryption block
    app_ini->randomBytes = generate_random_alphanumeric_string(encryption::BLOCK_SIZE, 654792);

    // read random initialization data
    QByteArray rand =
        base64_decode(settings.value("EncodedData/randomBytes", "").toByteArray());

    // if the randomblock is empty (first time app is run?)
    // then we should ask the user for new password and account details
    if (rand.size() != encryption::BLOCK_SIZE)
    {
        password_dialog npw;
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
        auto &bitstamp = bitstamp_network::get_bitstamp_instance()->account();
        bitstamp.network_ = bitstamp_network::get_bitstamp_instance();

        QByteArray API_user =
            base64_decode(settings.value("EncryptedData/API_user", "").toByteArray());
        bitstamp.API_user =
            encryptor.decrypt(secure_string(API_user.data(), API_user.size()));
        //
        QByteArray API_key =
            base64_decode(settings.value("EncryptedData/API_key", "").toByteArray());
        bitstamp.API_key =
            encryptor.decrypt(secure_string(API_key.data(), API_key.size()));
        if (std::getenv("Rand2")) {
            bitstamp.API_key = std::getenv("Rand2");
            std::cout << "Using key from ENV" << std::endl;
        }
        //
        QByteArray API_secret =
            base64_decode(settings.value("EncryptedData/API_secret", "").toByteArray());
        bitstamp.API_secret =
            encryptor.decrypt(secure_string(API_secret.data(), API_secret.size()));
        if (std::getenv("Rand3")) {
            bitstamp.API_secret = std::getenv("Rand3");
            std::cout << "Using sec from ENV" << std::endl;
        }
        //
        QByteArray API_tag_ =
            base64_decode(settings.value("EncryptedData/API_desttag", "").toByteArray());
        bitstamp.tag_ = std::atol(
            encryptor.decrypt(secure_string(API_tag_.data(), API_tag_.size())).c_str());

        //
        QByteArray API_public_ =
            base64_decode(settings.value("EncryptedData/API_xrpaddress", "").toByteArray());
        bitstamp.public_ =
            encryptor.decrypt(secure_string(API_public_.data(), API_public_.size()));

        // ---------------------------------------
        // XRP wallet details
        // ---------------------------------------
        app_ini->networks_.push_back(xrpl_network::get_instance(false));
        app_ini->networks_.push_back(xrpl_network::get_instance(true));
        //
        bool present = true;
        int index = 0;
        while (present) {
            QString num = QString::number(index);
            if (!settings.contains("EncryptedData/XRP_name_" + num)) present = false;
            else {
                ledger_wallet w;
                //
                bool XRP_testnet = settings.value("EncryptedData/XRP_test_" + num, "false").toBool();
                if (XRP_testnet) {
                    w.network_ = xrpl_network::get_xrpl_instance(true);
                    w.testnet_ = true;
                }
                else {
                    w.network_ = xrpl_network::get_xrpl_instance(false);
                    w.testnet_ = false;
                }
                w.tag_    = 0;
                w.widget_ = nullptr;
                //
                QByteArray XRP_name =
                    base64_decode(settings.value("EncryptedData/XRP_name_" + num, "").toByteArray());
                w.name_ =
                    encryptor.decrypt(secure_string(XRP_name.data(), XRP_name.size()));
                //
                QByteArray XRP_public =
                    base64_decode(settings.value("EncryptedData/XRP_public_" + num, "").toByteArray());
                w.public_ =
                    encryptor.decrypt(secure_string(XRP_public.data(), XRP_public.size()));
                //
                QByteArray XRP_secret =
                    base64_decode(settings.value("EncryptedData/XRP_secret_" + num, "").toByteArray());
                w.private_ =
                    encryptor.decrypt(secure_string(XRP_secret.data(), XRP_secret.size()));

                std::dynamic_pointer_cast<xrpl_network>(w.network_)->add_wallet(w);
            }
            index++;
        }
    }

    if (argc > 1 && std::string(argv[1]) == std::string("decode"))
    {
        auto &bitstamp = bitstamp_network::get_bitstamp_instance()->account();
        std::cout << "\nDecrypted information\n" << std::endl;
        std::cout << "API_user       : " << bitstamp.API_user << std::endl;
        std::cout << "API_key        : " << bitstamp.API_key << std::endl;
        std::cout << "API_secret     : " << bitstamp.API_secret << std::endl;
        std::cout << "xrp.tag        : " << bitstamp.tag_ << std::endl;
        std::cout << "xrp.public     : " << bitstamp.public_ << std::endl;
        //
        auto const & x1 = xrpl_network::get_xrpl_instance(false)->wallets();
        auto const & x2 = xrpl_network::get_xrpl_instance(true)->wallets();
        for (const auto lw : x1) {
            auto w = static_cast<ledger_wallet*>(lw);
            std::cout << "XRP_name       : " << w->name_ << std::endl;
            std::cout << "XRP_public     : " << w->public_ << std::endl;
            std::cout << "XRP_secret     : " << w->private_ << std::endl;
            std::cout << "XRP_testnet    : " << w->testnet_ << std::endl;
        }
        for (const auto lw : x2) {
            auto w = static_cast<ledger_wallet*>(lw);
            std::cout << "XRP_name       : " << w->name_ << std::endl;
            std::cout << "XRP_public     : " << w->public_ << std::endl;
            std::cout << "XRP_secret     : " << w->private_ << std::endl;
            std::cout << "XRP_testnet    : " << w->testnet_ << std::endl;
        }
        return EXIT_SUCCESS;
    }

    GroxMainWindow mainWindow;

    QObject::connect(
        &app, SIGNAL(aboutToQuit()), &mainWindow, SLOT(appExitCleanupHandler()));
    QObject::connect(&mainWindow, SIGNAL(quitApplication()), &app, SLOT(quit()));

    mainWindow.resize(1024, 768);
    mainWindow.show();

    return app.exec();
}
