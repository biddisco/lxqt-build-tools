#include <QApplication>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>

#include <memory>
#include <random>
#include <algorithm>

#include "mainwindow.hpp"
#include "password_dialog.hpp"
#include "settings.hpp"
#include "internet/evp-encrypt.hpp"

// ----------------------------------------------------------------------------
app_settings* global_settings()
{
    // create a global singleton and return an instance to it
    static std::unique_ptr<app_settings> settings =
        std::make_unique<app_settings>();
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
        (settings->configLocation + QLatin1String("/grox.ini"))
            .toLatin1()
            .data();
    std::cout << "Ini: " << settings->iniFileName.toLatin1().data()
              << std::endl;
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

secure_string base64_string(QByteArray ba)
{
    QByteArray bb = QByteArray::fromBase64(ba);
    return secure_string(bb.data(), bb.size());
}

std::string generate_random_alphanumeric_string(int seed, std::size_t len) {
    static constexpr auto chars =
        "0123456789"
        "~`!@#$%^&*()_-+={}[]|';:/?<>,."
        "!@#$%^&*(){}][:;'/?.>,<'`~| "
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    auto rng = std::mt19937(seed);
    auto dist = std::uniform_int_distribution{{}, std::strlen(chars) - 1};
    auto result = std::string(len, '\0');
    std::generate_n(begin(result), len, [&]() { return chars[dist(rng)]; });
    return result;
}

// ----------------------------------------------------------------------------
void generate_encrypted_ini_data(password_dialog &npw)
{
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    //
    app_ini->grox_password = npw.getPassword().toStdString();

    // we write a dummy random number to ini file
    // if this is present assume that the initial encryption step is valid
    constexpr int rand_size = encryption::BLOCK_SIZE;
    secure_string adummy_string = generate_random_alphanumeric_string(111111, rand_size);
    settings.setValue("EncodedData/randomBytes",
        QString::fromStdString(
            base64_encode(adummy_string).toStdString()));

    // -----------------------
    // Generate encrypted data
    // -----------------------
    encryption encryptor(app_ini->grox_password, app_ini->randomBytes);
    //
    app_ini->API_user   = npw.getAPIUser().toStdString();
    app_ini->API_key    = npw.getAPIKey().toStdString();
    app_ini->API_secret = npw.getAPISecret().toStdString();
    secure_string API_user   = encryptor.encrypt(app_ini->API_user);
    secure_string API_key    = encryptor.encrypt(app_ini->API_key);
    secure_string API_secret = encryptor.encrypt(app_ini->API_secret);
    //
    settings.setValue("EncryptedData/API_key",
        QString::fromStdString(base64_encode(API_key).toStdString()));
    settings.setValue("EncryptedData/API_user",
        QString::fromStdString(base64_encode(API_user).toStdString()));
    settings.setValue("EncryptedData/API_secret",
        QString::fromStdString(base64_encode(API_secret).toStdString()));
    //
    app_ini->XRP_name   = npw.getXRPName().toStdString();
    app_ini->XRP_public = npw.getXRPPublic().toStdString();
    app_ini->XRP_secret = npw.getXRPPrivate().toStdString();
    secure_string XRP_name   = encryptor.encrypt(app_ini->XRP_name);
    secure_string XRP_public = encryptor.encrypt(app_ini->XRP_public);
    secure_string XRP_secret = encryptor.encrypt(app_ini->XRP_secret);
    //
    settings.setValue("EncryptedData/XRP_name",
        QString::fromStdString(base64_encode(XRP_name).toStdString()));
    settings.setValue("EncryptedData/XRP_public",
        QString::fromStdString(base64_encode(XRP_public).toStdString()));
    settings.setValue("EncryptedData/XRP_secret",
        QString::fromStdString(base64_encode(XRP_secret).toStdString()));
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
    if (std::getenv("GROX_PASSWORD")!=nullptr) {
        app_ini->grox_password = std::getenv("GROX_PASSWORD");
    }
    else {
        std::cout << "Please set GROX_PASSWORD environment var" << std::endl;
        return EXIT_FAILURE;
    }

    constexpr int rand_size = encryption::BLOCK_SIZE;
    // we need random data for the encryption block
    app_ini->randomBytes = generate_random_alphanumeric_string(654792, rand_size);

    // read random initialization data
    QByteArray rand = base64_decode(
        settings.value("EncodedData/randomBytes", "").toByteArray());

    // if the randomblock is empty (first time app is run?)
    // then we should ask the user for new password and account details
    if (rand.size()!=rand_size) {
        password_dialog npw;
        if (npw.exec() == QDialog::Accepted) {
            generate_encrypted_ini_data(npw);
        }
    }
    else {
        // ---------------------------------------
        // decode and decrypt base64 keys
        // ---------------------------------------
        encryption encryptor(app_ini->grox_password, app_ini->randomBytes);

        // ---------------------------------------
        // Bitstamp exchange details
        // ---------------------------------------
        QByteArray API_user = base64_decode(
            settings.value("EncryptedData/API_user", "").toByteArray());
        app_ini->API_user =
            encryptor.decrypt(secure_string(API_user.data(), API_user.size()));
        //
        QByteArray API_key = base64_decode(
            settings.value("EncryptedData/API_key", "").toByteArray());
        app_ini->API_key =
            encryptor.decrypt(secure_string(API_key.data(), API_key.size()));
        //
        QByteArray API_secret = base64_decode(
            settings.value("EncryptedData/API_secret", "").toByteArray());
        app_ini->API_secret =
            encryptor.decrypt(secure_string(API_secret.data(), API_secret.size()));

        // ---------------------------------------
        // XRP walllet details
        // ---------------------------------------
        QByteArray XRP_name = base64_decode(
            settings.value("EncryptedData/XRP_name", "").toByteArray());
        app_ini->XRP_name =
            encryptor.decrypt(secure_string(XRP_name.data(), XRP_name.size()));
        //
        QByteArray XRP_public = base64_decode(
            settings.value("EncryptedData/XRP_public", "").toByteArray());
        app_ini->XRP_public =
            encryptor.decrypt(secure_string(XRP_public.data(), XRP_public.size()));
        //
        QByteArray XRP_secret = base64_decode(
            settings.value("EncryptedData/XRP_secret", "").toByteArray());
        app_ini->XRP_secret =
            encryptor.decrypt(secure_string(XRP_secret.data(), XRP_secret.size()));
    }

    if (argc>1 && std::string(argv[1])==std::string("decode")) {
        std::cout << "\nDecrypted information\n" << std::endl;
        std::cout << "API_user   : " << app_ini->API_user << std::endl;
        std::cout << "API_key    : " << app_ini->API_key << std::endl;
        std::cout << "API_secret : " << app_ini->API_secret << std::endl;
        std::cout << "XRP_name   : " << app_ini->XRP_name << std::endl;
        std::cout << "XRP_public : " << app_ini->XRP_public << std::endl;
        std::cout << "XRP_secret : " << app_ini->XRP_secret << std::endl;
        return EXIT_SUCCESS;
    }

    GroxMainWindow mainWindow;

    QObject::connect(&app, SIGNAL(aboutToQuit()), &mainWindow,
        SLOT(appExitCleanupHandler()));
    QObject::connect(
        &mainWindow, SIGNAL(quitApplication()), &app, SLOT(quit()));

    mainWindow.resize(1024, 768);
    mainWindow.show();

    return app.exec();
}
