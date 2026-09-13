#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QAbstractItemModel>

#include "io/transaction_store.hpp"

namespace grox {
  struct transactions_model_node;
}

namespace grox {

  // ----------------------------------------------------------------------------
  // Hierarchical read-only model for recent transactions.
  //
  // Market-trade rows (type == 2) that share the same order_id are grouped
  // under an expandable order node.  The order node displays aggregated
  // values: total amount, average fill price, total fee and number of fills.
  // Non-trade rows and trades without an order_id remain as top-level leaves.
  // ----------------------------------------------------------------------------
  class transactions_model : public QAbstractItemModel
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
      fills,
      column_count
    };

    explicit transactions_model(QObject* parent = nullptr);

    QModelIndex index(
        int row, int column, QModelIndex const& parent = QModelIndex()) const override;
    QModelIndex parent(QModelIndex const& child) const override;
    int rowCount(QModelIndex const& parent = QModelIndex()) const override;
    int columnCount(QModelIndex const& parent = QModelIndex()) const override;
    QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(
        int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void set_records(std::vector<transaction_record> records);
    std::vector<transaction_record> const& records() const { return records_; }

private:
    std::vector<transaction_record> records_;
    std::shared_ptr<transactions_model_node> root_;

    void rebuild_tree();
    static transactions_model_node const* node_from_index(QModelIndex const& index);
  };

}    // namespace grox
