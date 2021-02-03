#pragma once

#include <QDialog>
#include <QString>
//
#include <array>
#include <string>
//
#include "ui_password_dialog.h"

class password_dialog : public QDialog
{
    Q_OBJECT

public:
    password_dialog();
    password_dialog(const std::array<std::string,6> &strings);
    ~password_dialog();
    //
    QString getAPIUser();
    QString getAPIKey();
    QString getAPISecret();
    //
    QString getXRPName();
    QString getXRPPublic();
    QString getXRPPrivate();
    //
    QString getPassword();

private slots:
    void enable_ok_button();

private:
    Ui::password_dialog ui;
};
