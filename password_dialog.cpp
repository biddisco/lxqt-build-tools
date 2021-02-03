#include <QDialog>
#include <QFileInfo>
#include <QSettings>
#include <QString>
//
#include "password_dialog.hpp"

// ----------------------------------------------------------------------------
password_dialog::password_dialog()
  : QDialog()
{
    ui.setupUi(this);
    ui.ok_button->setEnabled(false);
    setWindowTitle("Wallet/Exchange Details");
    //
    connect(ui.api_user,    SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
    connect(ui.api_key,     SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
    connect(ui.api_secret,  SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
    connect(ui.xrp_nickname,SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
    connect(ui.xrp_public,  SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
    connect(ui.xrp_private, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
    connect(ui.password,    SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
    connect(ui.confirm,     SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
    connect(ui.ok_button,   SIGNAL(clicked()), this, SLOT(accept()));
}

password_dialog::password_dialog(const std::array<std::string,6> &strings) : password_dialog()
{
    ui.api_user->setText(QString(strings[0].c_str()));
    ui.api_key->setText(QString(strings[1].c_str()));
    ui.api_secret->setText(QString(strings[2].c_str()));
    ui.xrp_nickname->setText(QString(strings[3].c_str()));
    ui.xrp_public->setText(QString(strings[4].c_str()));
    ui.xrp_private->setText(QString(strings[5].c_str()));
}

password_dialog::~password_dialog() {}

QString password_dialog::getPassword()
{
    return ui.password->text();
}

QString password_dialog::getAPIUser()
{
    return ui.api_user->text();
}

QString password_dialog::getAPIKey()
{
    return ui.api_key->text();
}

QString password_dialog::getAPISecret()
{
    return ui.api_secret->text();
}

QString password_dialog::getXRPName()
{
    return ui.xrp_nickname->text();
}

QString password_dialog::getXRPPublic()
{
    return ui.xrp_public->text();
}

QString password_dialog::getXRPPrivate()
{
    return ui.xrp_private->text();
}

void password_dialog::enable_ok_button()
{
    if (ui.api_user->text().isEmpty() || ui.api_key->text().isEmpty() || ui.api_secret->text().isEmpty() || ui.password->text().isEmpty())
    {
        ui.ok_button->setEnabled(false);
        return;
    }
    if (ui.password->text() != ui.confirm->text())
    {
        ui.ok_button->setEnabled(false);
        return;
    }
    ui.ok_button->setEnabled(true);
}
