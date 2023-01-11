#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QString>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
//
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
    explicit connection_widget(QWidget *parent, net::contexts &io_contexts, exchange *ex);
    ~connection_widget();

    void setup_gui();

public slots:
    void filter_changed(const QString &s);
    void apply();

private:
    Ui::connection_widget *ui;
    net::contexts &io_contexts_;
    exchange *exchange_;
    QStandardItemModel *model_;
    QSortFilterProxyModel *filter_;
};
