#pragma once

#include <QWidget>
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
    explicit connection_widget(QWidget *parent = nullptr);
    explicit connection_widget(net::contexts &io_contexts, const exchange::exchange_vector &exchanges, QWidget *parent = nullptr);
    ~connection_widget();

    void connect_events();

private:
    Ui::connection_widget *ui;
};
