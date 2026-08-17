#include "widgets/transactions_model.hpp"

#include <QString>

namespace {

  QString format_double(double value)
  {
    if (value == 0.0) { return QString("0"); }
    QString s = QString::number(value, 'f', 8);
    while (s.endsWith('0') && s.contains('.')) { s.chop(1); }
    if (s.endsWith('.')) { s.chop(1); }
    return s.isEmpty() ? QString("0") : s;
  }

}    // namespace

namespace grox {

  // ----------------------------------------------------------------------------
  transactions_model::transactions_model(QObject* parent)
    : QAbstractTableModel(parent)
  {
  }

  // ----------------------------------------------------------------------------
  int transactions_model::rowCount(QModelIndex const& /*parent*/) const
  {
    return static_cast<int>(records_.size());
  }

  // ----------------------------------------------------------------------------
  int transactions_model::columnCount(QModelIndex const& /*parent*/) const { return column_count; }

  // ----------------------------------------------------------------------------
  QVariant transactions_model::data(QModelIndex const& index, int role) const
  {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(records_.size()))
    {
      return QVariant{};
    }

    auto const& rec = records_[index.row()];

    if (role == Qt::DisplayRole)
    {
      switch (index.column())
      {
      // Return numeric types for ID columns so the proxy sorts numerically.
      case id: return static_cast<qulonglong>(rec.id);
      case order_id:
        return rec.order_id.has_value() ? QVariant(static_cast<qulonglong>(*rec.order_id)) :
                                          QVariant{};
      case time: return QString::fromStdString(rec.datetime);
      case account: return QString::fromStdString(rec.account);
      case market: return QString::fromStdString(rec.market);
      case type: return rec.type;
      case side: return QString::fromStdString(rec.side);
      case amount:
        return rec.amount != 0.0 ?
            format_double(rec.amount) + " " + QString::fromStdString(rec.amount_ccy) :
            QVariant{};
      case price:
        return rec.price != 0.0 ?
            format_double(rec.price) + " " + QString::fromStdString(rec.price_ccy) :
            QVariant{};
      case fee:
        return rec.fee != 0.0 ? format_double(rec.fee) + " " + QString::fromStdString(rec.fee_ccy) :
                                QVariant{};
      case volume_usd:
        return rec.volume_usd.has_value() ? format_double(*rec.volume_usd) : QVariant{};
      default: return QVariant{};
      }
    }

    if (role == Qt::TextAlignmentRole)
    {
      if (index.column() == type) { return Qt::AlignCenter; }
      return Qt::AlignRight;
    }

    return QVariant{};
  }

  // ----------------------------------------------------------------------------
  QVariant transactions_model::headerData(int section, Qt::Orientation orientation, int role) const
  {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) { return QVariant{}; }

    switch (section)
    {
    case id: return QString("ID");
    case order_id: return QString("Order ID");
    case time: return QString("Time");
    case account: return QString("Account");
    case market: return QString("Market");
    case type: return QString("Type");
    case side: return QString("Side");
    case amount: return QString("Amount");
    case price: return QString("Price");
    case fee: return QString("Fee");
    case volume_usd: return QString("Volume USD");
    default: return QVariant{};
    }
  }

  // ----------------------------------------------------------------------------
  void transactions_model::set_records(std::vector<transaction_record> records)
  {
    beginResetModel();
    records_ = std::move(records);
    endResetModel();
  }

}    // namespace grox
