#ifndef wallet_widget_H
#define wallet_widget_H

#include <QWidget>
//
#include "src/settings.hpp"

namespace Ui {
class wallet_widget;
}

class wallet_widget : public QWidget
{
    Q_OBJECT

public:
    explicit wallet_widget(QWidget *parent = nullptr);
    ~wallet_widget();

    void set_data(ledger_wallet &w, int decimals=6);
    void set_data(bitstamp_account &w);

public slots:

private:
    Ui::wallet_widget *ui;
};

#endif // wallet_widget_H
