#include "widgets/transactions_model.hpp"

#include <QBrush>
#include <QColor>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

  QString format_double(double value)
  {
    if (value == 0.0) { return QString("0"); }
    QString s = QString::number(value, 'f', 8);
    while (s.endsWith('0') && s.contains('.')) { s.chop(1); }
    if (s.endsWith('.')) { s.chop(1); }
    return s.isEmpty() ? QString("0") : s;
  }

  QString format_double_2(double value)
  {
    if (value == 0.0) { return QString("0"); }
    QString s = QString::number(value, 'f', 2);
    // Remove trailing zeros after the 2 decimal places for exact values like 1.5000
    while (s.endsWith('0') && s.contains('.')) { s.chop(1); }
    if (s.endsWith('.')) { s.chop(1); }
    return s.isEmpty() ? QString("0") : s;
  }

  QString format_optional_double(std::optional<double> value)
  {
    return value.has_value() ? format_double(*value) : QString{};
  }

  QString transaction_type_name(int type)
  {
    switch (type)
    {
    case 0: return QString("Deposit");
    case 1: return QString("Withdrawal");
    case 2: return QString("Trade");
    case 14: return QString("Sub Account Transfer");
    case 25: return QString("Credited Staked");
    case 26: return QString("Sent To Staking");
    case 27: return QString("Staking Reward");
    case 32: return QString("Referral Reward");
    case 33: return QString("Settlement Transfer");
    case 35: return QString("Inter Account Transfer");
    case 58: return QString("Derivatives Settlement");
    case 59: return QString("Insurance Fund Claim");
    case 60: return QString("Insurance Fund Premium");
    case 61: return QString("Collateral Liquidation");
    default: return QString("Type %1").arg(type);
    }
  }

  QColor transaction_type_color(int type)
  {
    switch (type)
    {
    case 0: return QColor{34, 139, 34};     // Deposit -> forest green
    case 1: return QColor{178, 34, 34};     // Withdrawal -> firebrick red
    case 14: return QColor{128, 0, 128};    // Sub account transfer -> purple
    case 25:
    case 26:
    case 27: return QColor{0, 128, 128};    // Staking -> teal
    case 32: return QColor{255, 140, 0};    // Referral reward -> dark orange
    case 33:
    case 35: return QColor{128, 0, 128};    // Settlement/inter account -> purple
    case 58:
    case 59:
    case 60:
    case 61: return QColor{105, 105, 105};    // Derivatives/insurance -> dim gray
    default: return QColor{};
    }
  }

  QColor trade_side_color(std::string const& side)
  {
    if (side == "buy") { return QColor{0, 100, 200}; }       // Buy -> dark blue
    if (side == "sell") { return QColor{100, 180, 255}; }    // Sell -> light blue
    return QColor{};
  }

}    // namespace

namespace grox {

  // ------------------------------------------------------------------------
  // A single node in the transaction tree.  Order nodes own child fill nodes.
  // ------------------------------------------------------------------------
  struct transactions_model_node
  {
    bool is_order = false;
    std::size_t row = 0;
    transactions_model_node* parent = nullptr;
    std::vector<std::unique_ptr<transactions_model_node>> children;

    // Leaf data
    transaction_record const* record = nullptr;

    // Order aggregate data
    std::optional<std::uint64_t> order_id;
    std::string account;
    std::string datetime;
    std::string market;
    std::string side;
    std::string amount_ccy;
    std::string price_ccy;
    std::string fee_ccy;
    double total_amount = 0.0;
    double total_quote = 0.0;
    double total_fee = 0.0;
    std::optional<double> total_volume_usd;
    std::size_t fill_count = 0;

    [[nodiscard]] std::size_t child_count() const { return children.size(); }

    [[nodiscard]] transactions_model_node const* child_at(std::size_t r) const
    {
      return r < children.size() ? children[r].get() : nullptr;
    }

    [[nodiscard]] std::uint64_t id() const
    {
      if (!is_order && record != nullptr) { return record->id; }
      return order_id.value_or(0);
    }

    [[nodiscard]] std::optional<std::uint64_t> order_id_value() const
    {
      if (is_order) { return order_id; }
      if (record != nullptr) { return record->order_id; }
      return std::nullopt;
    }

    [[nodiscard]] QString datetime_display() const
    {
      if (!is_order && record != nullptr) { return QString::fromStdString(record->datetime); }
      return QString::fromStdString(datetime);
    }

    [[nodiscard]] QString account_display() const
    {
      if (!is_order && record != nullptr) { return QString::fromStdString(record->account); }
      return QString::fromStdString(account);
    }

    [[nodiscard]] QString market_display() const
    {
      if (!is_order && record != nullptr) { return QString::fromStdString(record->market); }
      return QString::fromStdString(market);
    }

    [[nodiscard]] int type_display() const
    {
      if (!is_order && record != nullptr) { return record->type; }
      // Order nodes only exist for market trades.
      return 2;
    }

    [[nodiscard]] QString type_name_display() const
    {
      if (!is_order && record != nullptr)
      {
        if (record->type == 2) { return QString::fromStdString(record->side); }
        return transaction_type_name(record->type);
      }
      if (is_order && type_display() == 2) { return QString::fromStdString(side); }
      return transaction_type_name(type_display());
    }

    [[nodiscard]] double amount_value() const
    {
      if (!is_order && record != nullptr) { return record->amount; }
      return total_amount;
    }

    [[nodiscard]] QString amount_ccy_display() const
    {
      if (!is_order && record != nullptr) { return QString::fromStdString(record->amount_ccy); }
      return QString::fromStdString(amount_ccy);
    }

    [[nodiscard]] QString price_display() const
    {
      if (!is_order && record != nullptr)
      {
        return record->price != 0.0 ?
            format_double_2(record->price) + " " + QString::fromStdString(record->price_ccy) :
            QString{};
      }
      if (total_amount == 0.0) { return QString{}; }
      return format_double_2(total_quote / total_amount) + " " + QString::fromStdString(price_ccy);
    }

    [[nodiscard]] QString fee_display() const
    {
      if (!is_order && record != nullptr)
      {
        return record->fee != 0.0 ?
            format_double_2(record->fee) + " " + QString::fromStdString(record->fee_ccy) :
            QString{};
      }
      return total_fee != 0.0 ? format_double_2(total_fee) + " " + QString::fromStdString(fee_ccy) :
                                QString{};
    }

    [[nodiscard]] std::optional<double> volume_usd_value() const
    {
      if (!is_order && record != nullptr) { return record->volume_usd; }
      return total_volume_usd;
    }

    [[nodiscard]] std::size_t fills_display() const
    {
      if (is_order) { return fill_count; }
      return 0;
    }

    [[nodiscard]] QColor type_color() const
    {
      // For market trades, let the side decide the foreground color.
      if (type_display() == 2)
      {
        if (!is_order && record != nullptr) { return trade_side_color(record->side); }
        return trade_side_color(side);
      }
      if (!is_order && record != nullptr) { return transaction_type_color(record->type); }
      return transaction_type_color(type_display());
    }
  };

  namespace {

    using node_type = transactions_model_node;

  }    // namespace

  // ----------------------------------------------------------------------------
  transactions_model::transactions_model(QObject* parent)
    : QAbstractItemModel(parent)
    , root_(std::make_shared<transactions_model_node>())
  {
  }

  // ----------------------------------------------------------------------------
  QModelIndex transactions_model::index(int row, int column, QModelIndex const& parent) const
  {
    if (!hasIndex(row, column, parent)) { return QModelIndex{}; }

    transactions_model_node const* parent_node =
        parent.isValid() ? node_from_index(parent) : root_.get();
    if (parent_node == nullptr) { return QModelIndex{}; }

    if (auto const* child = parent_node->child_at(static_cast<std::size_t>(row)))
    {
      return createIndex(row, column, const_cast<transactions_model_node*>(child));
    }
    return QModelIndex{};
  }

  // ----------------------------------------------------------------------------
  QModelIndex transactions_model::parent(QModelIndex const& child) const
  {
    if (!child.isValid()) { return QModelIndex{}; }

    auto const* child_node = node_from_index(child);
    if (child_node == nullptr || child_node->parent == nullptr || child_node->parent == root_.get())
    {
      return QModelIndex{};
    }

    return createIndex(static_cast<int>(child_node->parent->row), 0, child_node->parent);
  }

  // ----------------------------------------------------------------------------
  int transactions_model::rowCount(QModelIndex const& parent) const
  {
    if (parent.column() > 0) { return 0; }

    transactions_model_node const* parent_node =
        parent.isValid() ? node_from_index(parent) : root_.get();
    if (parent_node == nullptr) { return 0; }

    return static_cast<int>(parent_node->child_count());
  }

  // ----------------------------------------------------------------------------
  int transactions_model::columnCount(QModelIndex const& /*parent*/) const { return column_count; }

  // ----------------------------------------------------------------------------
  QVariant transactions_model::data(QModelIndex const& index, int role) const
  {
    if (!index.isValid()) { return QVariant{}; }

    auto const* n = node_from_index(index);
    if (n == nullptr) { return QVariant{}; }

    if (role == Qt::DisplayRole)
    {
      switch (index.column())
      {
      case id: return n->is_order ? QVariant{} : QVariant{static_cast<qulonglong>(n->id())};
      case order_id:
      {
        auto const oid = n->order_id_value();
        return oid.has_value() ? QVariant{static_cast<qulonglong>(*oid)} : QVariant{};
      }
      case time: return n->datetime_display();
      case account: return n->account_display();
      case market: return n->market_display();
      case type: return n->type_display();
      case side: return n->type_name_display();
      case amount:
      {
        double const v = n->amount_value();
        return v != 0.0 ? format_double(v) + " " + n->amount_ccy_display() : QVariant{};
      }
      case price: return n->price_display();
      case fee: return n->fee_display();
      case volume_usd: return format_optional_double(n->volume_usd_value());
      case fills:
      {
        auto const f = n->fills_display();
        return f != 0 ? QVariant{static_cast<qulonglong>(f)} : QVariant{};
      }
      default: return QVariant{};
      }
    }

    if (role == Qt::TextAlignmentRole)
    {
      if (index.column() == type) { return Qt::AlignCenter; }
      return Qt::AlignRight;
    }

    if (role == Qt::ForegroundRole)
    {
      auto const color = n->type_color();
      if (color.isValid()) { return QBrush{color}; }
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
    case side: return QString("Type Name");
    case amount: return QString("Amount");
    case price: return QString("Price");
    case fee: return QString("Fee");
    case volume_usd: return QString("Volume USD");
    case fills: return QString("Fills");
    default: return QVariant{};
    }
  }

  // ----------------------------------------------------------------------------
  void transactions_model::set_records(std::vector<transaction_record> records)
  {
    beginResetModel();
    records_ = std::move(records);
    rebuild_tree();
    endResetModel();
  }

  // ----------------------------------------------------------------------------
  void transactions_model::rebuild_tree()
  {
    root_ = std::make_shared<transactions_model_node>();

    struct order_key
    {
      std::string account;
      std::uint64_t order_id = 0;

      bool operator==(order_key const& other) const noexcept
      {
        return order_id == other.order_id && account == other.account;
      }
    };

    struct order_key_hash
    {
      std::size_t operator()(order_key const& k) const noexcept
      {
        return std::hash<std::uint64_t>{}(k.order_id) ^ std::hash<std::string>{}(k.account);
      }
    };

    std::unordered_map<order_key, transactions_model_node*, order_key_hash> order_map;

    for (auto const& rec : records_)
    {
      bool const is_trade = rec.type == 2;
      bool const has_order_id = rec.order_id.has_value();

      if (!is_trade || !has_order_id)
      {
        auto leaf = std::make_unique<transactions_model_node>();
        leaf->record = &rec;
        leaf->row = root_->children.size();
        leaf->parent = root_.get();
        root_->children.push_back(std::move(leaf));
        continue;
      }

      order_key const key{rec.account, *rec.order_id};
      auto it = order_map.find(key);
      transactions_model_node* order_node = nullptr;
      if (it == order_map.end())
      {
        auto order = std::make_unique<transactions_model_node>();
        order->is_order = true;
        order->order_id = rec.order_id;
        order->account = rec.account;
        order->datetime = rec.datetime;
        order->market = rec.market;
        order->side = rec.side;
        order->amount_ccy = rec.amount_ccy;
        order->price_ccy = rec.price_ccy;
        order->fee_ccy = rec.fee_ccy;
        order->row = root_->children.size();
        order->parent = root_.get();

        order_node = order.get();
        order_map.emplace(key, order_node);
        root_->children.push_back(std::move(order));
      }
      else
      {
        order_node = it->second;
        if (rec.datetime < order_node->datetime) { order_node->datetime = rec.datetime; }
      }

      auto fill = std::make_unique<transactions_model_node>();
      fill->record = &rec;
      fill->row = order_node->children.size();
      fill->parent = order_node;
      order_node->children.push_back(std::move(fill));

      order_node->total_amount += rec.amount;
      order_node->total_quote += rec.amount * rec.price;
      order_node->total_fee += rec.fee;
      order_node->fill_count += 1;
      if (rec.volume_usd.has_value())
      {
        order_node->total_volume_usd = order_node->total_volume_usd.value_or(0.0) + *rec.volume_usd;
      }
    }
  }

  // ----------------------------------------------------------------------------
  transactions_model_node const* transactions_model::node_from_index(QModelIndex const& index)
  {
    return static_cast<transactions_model_node const*>(index.internalPointer());
  }

}    // namespace grox
