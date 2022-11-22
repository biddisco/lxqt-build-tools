#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
//
#include <string_view>

#include "src/exchange/exchange.hpp"
#include "src/currency.hpp"
#include "src/trade_data.hpp"

namespace Ui {
    class connection_widget;
}

class connection_widget : public QWidget
{
    Q_OBJECT

public:
//    explicit connection_widget(QWidget *parent = nullptr);
    explicit connection_widget(net::contexts &io_contexts, QWidget *parent = nullptr);
    ~connection_widget();

    void connect_events();

    QGroupBox *get_stream_box();
    QGroupBox *get_ticker_box();

public slots:
    void update_exchange_data(exchange *ex);

private:
    Ui::connection_widget *ui;
//    QVBoxLayout* net_layout_;
    net::contexts &io_contexts_;

};
