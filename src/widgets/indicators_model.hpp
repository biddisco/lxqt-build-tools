#pragma once

#include <vector>
//
#include <QAbstractTableModel>
//
#include "indicators/indicator_ptr.hpp"

// ----------------------------------------------------------------------------
class indicators_model : public QAbstractTableModel
{
  Q_OBJECT

  public:
  explicit indicators_model(QObject* parent = nullptr);

  int rowCount(QModelIndex const& parent = QModelIndex()) const override;
  int columnCount(QModelIndex const& parent = QModelIndex()) const override;
  QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;

  void dataAdded();

  std::vector<indicators::indicator_ptr> indicators_;
};