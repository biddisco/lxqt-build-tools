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
    currency currency_;

public:
    explicit currency_widget(QWidget *parent = nullptr);
    ~currency_widget();

    void set_data(currency &c);

public slots:
    // ----------------------------------
    void transfer_setup_xrp(double);
    //
    void q1x_clicked();
    void q2x_clicked();
    void q3x_clicked();
    void q4x_clicked();
private:
    Ui::currency_widget *ui;
};

#endif // CURRENCY_WIDGET_H
