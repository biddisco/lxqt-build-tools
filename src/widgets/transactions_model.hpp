#pragma once

#include <vector>

#include <QAbstractTableModel>

#include "io/transaction_store.hpp"

namespace grox {

  // ----------------------------------------------------------------------------
  // Read-only table model for the recent transactions stored in the SQLite
  // transaction store.  The backing data is replaced wholesale by set_records().
  // ----------------------------------------------------------------------------
  class transactions_model : public QAbstractTableModel
  {
    Q_OBJECT

public:
    enum columns
    {
      id = 0,
      order_id,
      time,
      account,
      market,
      type,
      side,
      amount,
      price,
      fee,
      volume_usd,
      column_count
    };

    explicit transactions_model(QObject* parent = nullptr);

    int rowCount(QModelIndex const& parent = QModelIndex()) const override;
    int columnCount(QModelIndex const& parent = QModelIndex()) const override;
    QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void set_records(std::vector<transaction_record> records);
    std::vector<transaction_record> const& records() const { return records_; }

private:
    std::vector<transaction_record> records_;
  };

}    // namespace grox
