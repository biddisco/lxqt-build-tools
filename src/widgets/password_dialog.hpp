#pragma once

#include <QDialog>
#include <QString>
//
#include <vector>
#include <string>
//
#include "ui_password_dialog.h"
//
#include "src/settings.hpp"

class password_dialog : public QDialog
{
    Q_OBJECT
    std::vector<ledger_wallet> wallets_;

public:
    password_dialog(bool simple);
    password_dialog(const std::array<std::string, 5>& strings,
                    const std::vector<ledger_wallet> &wallets);
    ~password_dialog();

    // Exchange details
    QString getAPIUser();
    QString getAPIKey();
    QString getAPISecret();
    QString getAPIDestTag();
    QString getAPIXRPAddress();

    // Wallet details
    const std::vector<ledger_wallet> &get_wallets();
    //
    QString getPassword();
    //

private slots:
    void enable_ok_button();
    void add_wallet();
    void remove_wallet();
    void refresh_gui(int index);

private:
    Ui::password_dialog ui;
    bool simple_mode_;
};
