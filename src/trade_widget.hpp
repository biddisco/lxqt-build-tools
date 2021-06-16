#pragma once

#include <QWidget>
#include <string_view>

#include "src/exchange/exchange.hpp"
#include "currency.hpp"
#include "trade_data.hpp"

namespace Ui {
class trade_widget;
}

class trade_widget : public QWidget
{
    Q_OBJECT

public:
    explicit trade_widget(QWidget *parent = nullptr);
    explicit trade_widget(std::string_view data, QWidget *parent = nullptr);
    ~trade_widget();

    void set_data(trade_data const &t);

private:
    Ui::trade_widget *ui;
};
