#ifndef CURRENCY_WIDGET_H
#define CURRENCY_WIDGET_H

#include <QWidget>
//
#include "settings.hpp"

namespace Ui {
class currency_widget;
}

class currency_widget : public QWidget
{
    Q_OBJECT

private:
    Ui::currency_widget *ui;
    int            decimals_;
    currency       currency_;
    basic_account *account_;
    exchange      *network_;
    double         amount_;
    std::string    fmt_;

public:
    currency_widget(int decimals, QWidget *parent = nullptr);
    ~currency_widget();

    void set_data(currency const *c, basic_account *acct=nullptr, exchange *network=nullptr);

public slots:
    // ----------------------------------
    void transfer_setup_xrp(double);
    //
    void q1x_clicked();
    void q2x_clicked();
    void q3x_clicked();
    void q4x_clicked();

    void show_hide();
    double get_amount();
    void execute_transfer();
};

#endif // CURRENCY_WIDGET_H
