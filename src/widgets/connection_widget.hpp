#pragma once

#include <QGroupBox>
#include <QListWidget>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
//
#include "currency/currency.hpp"
#include "exchange/exchange.hpp"

namespace Ui {
  class connection_widget;
}

class connection_widget : public QWidget
{
  Q_OBJECT

  public:
  explicit connection_widget(QWidget* parent, net::contexts& io_contexts, exchange* ex);
  ~connection_widget();

  void setup_gui();

  public slots:
  void filter_changed(QString const& s);
  void apply();

  private:
  Ui::connection_widget* ui;
  net::contexts& io_contexts_;
  exchange* exchange_;
  QStandardItemModel* model_;
  QSortFilterProxyModel* filter_;
};
