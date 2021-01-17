#include <QApplication>
#include <QObject>
#include <QStandardPaths>
#include <QSettings>

#include <memory>

#include "mainwindow.hpp"
#include "settings.hpp"
#include "internet/evp-encrypt.hpp"

// ----------------------------------------------------------------------------
app_settings *global_settings()
{
    // create a global singleton and return an instance to it
    static std::unique_ptr<app_settings> settings = std::make_unique<app_settings>();
    return settings.get();
}

// ----------------------------------------------------------------------------
void init_settings(app_settings *settings) {
    settings->tempLocation    = QStandardPaths::standardLocations(QStandardPaths::TempLocation).first().toLatin1().data();
    settings->configLocation  = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first().toLatin1().data();
    settings->appDataLocation = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first().toLatin1().data();
    //
    settings->hdfFileName     = settings->appDataLocation + "/grox.hdf5";
    settings->logFileName     = QLatin1String("grox.log").data();
    settings->iniFileName     = (settings->configLocation + QLatin1String("/grox.ini")).toLatin1().data();
    std::cout << "Ini: " << settings->iniFileName.toLatin1().data() << std::endl;
}

QByteArray base64_encode(const QByteArray &ba){
    return ba.toBase64();
}

QByteArray base64_encode(const secure_string &s){
    QByteArray ba(s.data(), s.size());
    return ba.toBase64();
}

QByteArray base64_decode(QByteArray ba) {
    return QByteArray::fromBase64(ba);
}

secure_string base64_string(QByteArray ba) {
    QByteArray bb = QByteArray::fromBase64(ba);
    return secure_string(bb.data(), bb.size());
}
// ----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QIcon icon(":images/xrp.ico");
    app.setWindowIcon(icon);
    app.setApplicationName("grox");
    //
    init_settings(global_settings());
    QSettings settings(global_settings()->iniFileName, QSettings::IniFormat);
    //
    app_settings *app_ini = global_settings();
    app_ini->randomPassword = "string that should be supplied by user";
    // these are needed to init the encryption block + padding space etc
    QByteArray rand         = base64_decode(settings.value("EncodedData/randomBytes", "").toByteArray());
    app_ini->randomBytes    = secure_string(rand.data(), rand.size());
    encryption encryptor(app_ini->randomPassword, app_ini->randomBytes);


    // use DECODE_MODE 1 to generate plaintext keys
    // use DECODE_MODE 2 to encrypt the keys generated in '1' and write them to ini
    // use DECODE_MODE 0 when '1' and '2' are done and all is working as expected
    QByteArray restKey;
    QByteArray restSign;
    QByteArray ApiKeySign;

#define DECODE_MODE 0
#if DECODE_MODE==1
    // write base64 plaintext keys

#elif DECODE_MODE==2
    // decode base64 plaintext keys, then encrypt + re-encode as base64 and write
    restKey    = base64_decode(settings.value("EncodedData/restKey", "").toByteArray());
    restSign   = base64_decode(settings.value("EncodedData/restSign", "").toByteArray());
    ApiKeySign = base64_decode(settings.value("EncodedData/ApiKeySign", "").toByteArray());
    //
    secure_string restKey_s    = secure_string(restKey.data(), restKey.size());
    secure_string restSign_s   = secure_string(restSign.data(), restSign.size());
    secure_string ApiKeySign_s = secure_string(ApiKeySign.data(), ApiKeySign.size());
    //
    restKey_s      = encryptor.encrypt(restKey_s);
    restSign_s     = encryptor.encrypt(restSign_s);
    ApiKeySign_s   = encryptor.encrypt(ApiKeySign_s);
    //
    settings.setValue("EncodedData/randomBytes",    QString::fromStdString(base64_encode(app_ini->randomBytes).toStdString()));
    settings.setValue("EncryptedData/restKey",      QString::fromStdString(base64_encode(restKey_s).toStdString()));
    settings.setValue("EncryptedData/restSign",     QString::fromStdString(base64_encode(restSign_s).toStdString()));
    settings.setValue("EncryptedData/ApiKeySign",   QString::fromStdString(base64_encode(ApiKeySign_s).toStdString()));
#endif

    // unencode and decrypt base64 keys
    restKey             = base64_decode(settings.value("EncryptedData/restKey", "").toByteArray());
    app_ini->restKey    = encryptor.decrypt(secure_string(restKey.data(), restKey.size()));
    restSign            = base64_decode(settings.value("EncryptedData/restSign", "").toByteArray());
    app_ini->restSign   = encryptor.decrypt(secure_string(restSign.data(), restSign.size()));
    ApiKeySign          = base64_decode(settings.value("EncryptedData/ApiKeySign", "").toByteArray());
    app_ini->ApiKeySign = encryptor.decrypt(secure_string(ApiKeySign.data(), ApiKeySign.size()));

    GroxMainWindow mainWindow;

    QObject::connect(&app, SIGNAL(aboutToQuit()), &mainWindow, SLOT(appExitCleanupHandler()));
    QObject::connect(&mainWindow, SIGNAL(quitApplication()), &app, SLOT(quit()));

    mainWindow.resize( 1024, 768 );
    mainWindow.show();

    return app.exec();
}
