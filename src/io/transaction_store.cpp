#include "io/transaction_store.hpp"

#include <QDateTime>
#include <QTimeZone>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fmt/format.h>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include "util/stringutils.hpp"

namespace grox {

  namespace {

    constexpr char const* table_name = "bitstamp_user_transactions";

    struct market_descriptor
    {
      std::string price_key;
      std::string base_ccy;
      std::string quote_ccy;
    };

    // Layout used for SELECT queries.  SQLite/SOCI rowset<row> stores integers
    // in 32-bit holders, which truncates large IDs and timestamps, so we bind
    // integer columns directly into 64-bit fields via soci::into().
    struct transaction_row
    {
      long long id = 0;
      std::string account;
      std::string datetime;
      long long timestamp_ms = 0;
      long long type = 0;
      long long order_id = 0;
      soci::indicator order_id_ind = soci::i_null;
      std::string market;
      std::string side;
      double amount = 0.0;
      std::string amount_ccy;
      double price = 0.0;
      std::string price_ccy;
      double fee = 0.0;
      std::string fee_ccy;
      double volume_usd = 0.0;
      soci::indicator volume_usd_ind = soci::i_null;
      std::string raw_json;
    };

    // The price fields that appear in Bitstamp /api/v2/user_transactions/ responses.
    // Order matters: more specific / commonly traded pairs are listed first.
    std::vector<market_descriptor> const bitstamp_markets{
        {"xrp_usd", "XRP", "USD"},
        {"xrp_eur", "XRP", "EUR"},
        {"xrp_gbp", "XRP", "GBP"},
        {"eth_usd", "ETH", "USD"},
        {"eth_eur", "ETH", "EUR"},
    };

    // Convert a Bitstamp currency key such as "xrp", "usd", ... to an uppercase code.
    std::string ccy_from_key(std::string const& key)
    {
      std::string out = key;
      std::transform(
          out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::toupper(c); });
      return out;
    }

    std::string bitstamp_market_from_keys(nlohmann::json const& j)
    {
      for (auto const& m : bitstamp_markets)
      {
        if (j.contains(m.price_key))
        {
          auto v = j.at(m.price_key);
          if (v.is_number() && v.get<double>() != 0.0) { return m.base_ccy + "/" + m.quote_ccy; }
          if (v.is_string())
          {
            std::string s = v.get<std::string>();
            if (!s.empty() && s != "null" && s != "0.00000000" && s != "0" && s != "0.0")
            {
              return m.base_ccy + "/" + m.quote_ccy;
            }
          }
        }
      }
      return {};
    }

  }    // namespace

  // ----------------------------------------------------------------------------
  transaction_store::transaction_store() = default;

  // ----------------------------------------------------------------------------
  transaction_store::transaction_store(std::string const& db_path) { open(db_path); }

  // ----------------------------------------------------------------------------
  transaction_store::~transaction_store() { close(); }

  // ----------------------------------------------------------------------------
  void transaction_store::open(std::string const& db_path)
  {
    std::lock_guard<pika::detail::spinlock> lock(mutex_);
    close();
    sql_ = std::make_unique<soci::session>(soci::sqlite3, db_path);
    init_schema();
  }

  // ----------------------------------------------------------------------------
  void transaction_store::close()
  {
    if (sql_) { sql_->close(); }
    sql_.reset();
  }

  // ----------------------------------------------------------------------------
  void transaction_store::init_schema()
  {
    *sql_ << fmt::format(
        R"sql(
        CREATE TABLE IF NOT EXISTS {} (
          id            INTEGER NOT NULL,
          account       TEXT    NOT NULL,
          datetime      TEXT    NOT NULL,
          timestamp_ms  INTEGER NOT NULL,
          type          INTEGER NOT NULL,
          order_id      INTEGER,
          market        TEXT,
          side          TEXT,
          amount        REAL,
          amount_ccy    TEXT,
          price         REAL,
          price_ccy     TEXT,
          fee           REAL,
          fee_ccy       TEXT,
          volume_usd    REAL,
          raw_json      TEXT,
          PRIMARY KEY (id, account)
        )
      )sql",
        table_name);

    *sql_ << fmt::format(
        "CREATE INDEX IF NOT EXISTS idx_{}_account_datetime ON {} (account, datetime DESC);",
        table_name, table_name);

    // Better concurrency for a read-heavy GUI while C++ appends transactions.
    *sql_ << "PRAGMA journal_mode=WAL;";
  }

  // ----------------------------------------------------------------------------
  std::optional<double> transaction_store::json_opt_double(
      nlohmann::json const& j, std::string const& key)
  {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) { return std::nullopt; }
    if (it->is_number()) { return it->get<double>(); }
    if (!it->is_string()) { return std::nullopt; }
    std::string s = it->get<std::string>();
    if (s.empty() || s == "null" || s == "<NA>") { return std::nullopt; }
    try
    {
      return std::stod(s);
    }
    catch (...)
    {
      return std::nullopt;
    }
  }

  // ----------------------------------------------------------------------------
  std::pair<std::string, std::int64_t> transaction_store::parse_datetime(
      std::string const& datetime)
  {
    // Bitstamp datetimes are "yyyy-MM-dd hh:mm:ss[.ffffff]". Normalize to a
    // fixed six-digit fractional format and compute the UTC millisecond timestamp.
    std::string base = datetime.size() >= 19 ? datetime.substr(0, 19) : datetime;
    std::string frac = "000000";
    if (datetime.size() > 19 && datetime[19] == '.')
    {
      frac = datetime.substr(20);
      if (frac.size() > 6) { frac = frac.substr(0, 6); }
      while (frac.size() < 6) { frac.push_back('0'); }
    }

    QDateTime dt = QDateTime::fromString(QString::fromStdString(base), "yyyy-MM-dd hh:mm:ss");
    if (!dt.isValid()) { return {datetime, 0}; }
    dt.setTimeZone(QTimeZone::UTC);

    std::int64_t const ms = dt.toMSecsSinceEpoch();
    std::int64_t const fractional_ms = std::stoll(frac) / 1000;
    return {base + "." + frac, ms + fractional_ms};
  }

  // ----------------------------------------------------------------------------
  void transaction_store::insert_bitstamp_transactions(
      std::string const& account, nlohmann::json const& jdata)
  {
    if (!jdata.is_array()) { return; }

    std::lock_guard<pika::detail::spinlock> lock(mutex_);
    if (!sql_) { return; }

    std::string const insert_sql = fmt::format(
        R"sql(
        INSERT OR REPLACE INTO {} (
          id, account, datetime, timestamp_ms, type, order_id, market, side,
          amount, amount_ccy, price, price_ccy, fee, fee_ccy, volume_usd, raw_json
        ) VALUES (
          :id, :account, :datetime, :timestamp_ms, :type, :order_id, :market, :side,
          :amount, :amount_ccy, :price, :price_ccy, :fee, :fee_ccy, :volume_usd, :raw_json
        )
      )sql",
        table_name);

    for (auto const& item : jdata)
    {
      if (!item.is_object()) { continue; }

      std::uint64_t id = 0;
      if (item.contains("id"))
      {
        auto const& idv = item.at("id");
        if (idv.is_number_unsigned()) { id = idv.get<std::uint64_t>(); }
        else if (idv.is_string()) { id = std::stoull(idv.get<std::string>()); }
      }

      std::string datetime;
      if (item.contains("datetime") && item.at("datetime").is_string())
      {
        datetime = item.at("datetime").get<std::string>();
      }

      std::int32_t type = 0;
      if (item.contains("type"))
      {
        auto const& tv = item.at("type");
        if (tv.is_number_integer()) { type = tv.get<std::int32_t>(); }
        else if (tv.is_string()) { type = std::stoi(tv.get<std::string>()); }
      }

      std::optional<std::uint64_t> order_id;
      if (item.contains("order_id") && !item.at("order_id").is_null())
      {
        auto const& oid = item.at("order_id");
        if (oid.is_number()) { order_id = oid.get<std::uint64_t>(); }
        else if (oid.is_string()) { order_id = std::stoull(oid.get<std::string>()); }
      }

      // Detect the market / pair from the price fields present in the JSON.
      std::string market;
      std::string amount_ccy;
      std::string price_ccy;
      std::string side;
      double amount = 0.0;
      double price = 0.0;

      for (auto const& m : bitstamp_markets)
      {
        auto opt_price = json_opt_double(item, m.price_key);
        auto opt_base = json_opt_double(item, lowercase(m.base_ccy));
        if (opt_price.has_value())
        {
          market = m.base_ccy + "/" + m.quote_ccy;
          price_ccy = m.quote_ccy;
          price = *opt_price;
          if (opt_base.has_value())
          {
            amount_ccy = m.base_ccy;
            amount = std::fabs(*opt_base);
            side = (*opt_base > 0.0) ? "buy" : "sell";
          }
          break;
        }
      }

      // Non-trade entries (deposits, withdrawals, transfers) do not have a price
      // field.  Store the single non-zero currency amount we can find.
      if (market.empty())
      {
        for (auto const& key : {"usd", "eur", "gbp", "xrp", "eth"})
        {
          auto v = json_opt_double(item, key);
          if (v.has_value() && std::fabs(*v) > 0.0)
          {
            amount = std::fabs(*v);
            amount_ccy = ccy_from_key(key);
            break;
          }
        }
      }

      double fee = json_opt_double(item, "fee").value_or(0.0);
      std::string fee_ccy = price_ccy.empty() ? amount_ccy : price_ccy;

      // Compute a USD notional for fee-tier volume tracking where we have enough
      // information to do so.  EUR/GBP only trades will leave volume_usd NULL
      // until we add an FX rate cache (see TODO).
      std::optional<double> volume_usd;
      if (!market.empty())
      {
        if (price_ccy == "USD") { volume_usd = amount * price; }
        else if (amount_ccy == "XRP")
        {
          auto xrp_usd = json_opt_double(item, "xrp_usd");
          if (xrp_usd.has_value() && *xrp_usd != 0.0) { volume_usd = amount * (*xrp_usd); }
        }
      }
      if (!volume_usd.has_value())
      {
        auto usd = json_opt_double(item, "usd");
        if (usd.has_value() && *usd != 0.0) { volume_usd = std::fabs(*usd); }
      }

      soci::indicator order_id_ind = order_id ? soci::i_ok : soci::i_null;
      std::uint64_t order_id_val = order_id.value_or(0);

      soci::indicator volume_usd_ind = volume_usd ? soci::i_ok : soci::i_null;
      double volume_usd_val = volume_usd.value_or(0.0);

      std::string raw_json = item.dump();
      auto const [normalized_datetime, timestamp_ms] = parse_datetime(datetime);

      *sql_ << insert_sql, soci::use(id), soci::use(account), soci::use(normalized_datetime),
          soci::use(timestamp_ms), soci::use(type), soci::use(order_id_val, order_id_ind),
          soci::use(market), soci::use(side), soci::use(amount), soci::use(amount_ccy),
          soci::use(price), soci::use(price_ccy), soci::use(fee), soci::use(fee_ccy),
          soci::use(volume_usd_val, volume_usd_ind), soci::use(raw_json);
    }
  }

  // ----------------------------------------------------------------------------
  std::uint64_t transaction_store::last_transaction_id(std::string const& account) const
  {
    std::lock_guard<pika::detail::spinlock> lock(mutex_);
    if (!sql_) { return 0; }

    std::uint64_t id = 0;
    soci::indicator ind = soci::i_null;
    *sql_ << fmt::format("SELECT MAX(id) FROM {} WHERE account = :account", table_name),
        soci::use(account), soci::into(id, ind);
    return ind == soci::i_ok ? id : 0;
  }

  // ----------------------------------------------------------------------------
  std::vector<transaction_record> transaction_store::recent_transactions(std::size_t limit) const
  {
    return transactions_in_range("", "", "", limit);
  }

  // ----------------------------------------------------------------------------
  std::vector<transaction_record> transaction_store::recent_transactions(
      std::string const& account, std::size_t limit) const
  {
    return transactions_in_range(account, "", "", limit);
  }

  // ----------------------------------------------------------------------------
  std::vector<transaction_record> transaction_store::transactions_in_range(
      std::string const& start_datetime, std::string const& end_datetime, std::size_t limit) const
  {
    return transactions_in_range("", start_datetime, end_datetime, limit);
  }

  // ----------------------------------------------------------------------------
  std::vector<transaction_record> transaction_store::transactions_in_range(
      std::string const& account, std::string const& start_datetime,
      std::string const& end_datetime, std::size_t limit) const
  {
    std::lock_guard<pika::detail::spinlock> lock(mutex_);
    std::vector<transaction_record> result;
    if (!sql_) { return result; }

    std::string const start = start_datetime.empty() ? "1970-01-01 00:00:00" : start_datetime;
    std::string const end = end_datetime.empty() ? "9999-12-31 23:59:59" : end_datetime;
    bool const filter_account = !account.empty();

    std::string query =
        fmt::format("SELECT id, account, datetime, timestamp_ms, type, order_id, market, side, "
                    "amount, amount_ccy, price, price_ccy, fee, fee_ccy, volume_usd, raw_json "
                    "FROM {} WHERE datetime >= :start AND datetime <= :end{} "
                    "ORDER BY datetime DESC LIMIT :limit",
            table_name, filter_account ? " AND account = :account" : "");

    transaction_row row;

    soci::statement st = filter_account ?
        (sql_->prepare << query, soci::into(row.id), soci::into(row.account),
            soci::into(row.datetime), soci::into(row.timestamp_ms), soci::into(row.type),
            soci::into(row.order_id, row.order_id_ind), soci::into(row.market),
            soci::into(row.side), soci::into(row.amount), soci::into(row.amount_ccy),
            soci::into(row.price), soci::into(row.price_ccy), soci::into(row.fee),
            soci::into(row.fee_ccy), soci::into(row.volume_usd, row.volume_usd_ind),
            soci::into(row.raw_json), soci::use(start), soci::use(end), soci::use(account),
            soci::use(static_cast<int>(limit))) :
        (sql_->prepare << query, soci::into(row.id), soci::into(row.account),
            soci::into(row.datetime), soci::into(row.timestamp_ms), soci::into(row.type),
            soci::into(row.order_id, row.order_id_ind), soci::into(row.market),
            soci::into(row.side), soci::into(row.amount), soci::into(row.amount_ccy),
            soci::into(row.price), soci::into(row.price_ccy), soci::into(row.fee),
            soci::into(row.fee_ccy), soci::into(row.volume_usd, row.volume_usd_ind),
            soci::into(row.raw_json), soci::use(start), soci::use(end),
            soci::use(static_cast<int>(limit)));

    st.execute();
    while (st.fetch())
    {
      transaction_record rec;
      rec.id = static_cast<std::uint64_t>(row.id);
      rec.account = row.account;
      rec.datetime = row.datetime;
      rec.timestamp_ms = row.timestamp_ms;
      rec.type = static_cast<std::int32_t>(row.type);
      rec.order_id = row.order_id_ind == soci::i_ok ?
          std::optional<std::uint64_t>(static_cast<std::uint64_t>(row.order_id)) :
          std::nullopt;
      rec.market = row.market;
      rec.side = row.side;
      rec.amount = row.amount;
      rec.amount_ccy = row.amount_ccy;
      rec.price = row.price;
      rec.price_ccy = row.price_ccy;
      rec.fee = row.fee;
      rec.fee_ccy = row.fee_ccy;
      rec.volume_usd =
          row.volume_usd_ind == soci::i_ok ? std::optional<double>(row.volume_usd) : std::nullopt;
      rec.raw_json = row.raw_json;
      result.push_back(rec);
    }
    return result;
  }

  // ----------------------------------------------------------------------------
  volume_summary transaction_store::rolling_volume(
      std::string const& account, std::chrono::system_clock::time_point now) const
  {
    std::lock_guard<pika::detail::spinlock> lock(mutex_);
    volume_summary summary;
    summary.account = account;
    summary.since = now - std::chrono::days(30);
    if (!sql_) { return summary; }

    double volume = 0.0;
    soci::indicator ind = soci::i_null;
    *sql_ << fmt::format("SELECT COALESCE(SUM(volume_usd), 0) FROM {} "
                         "WHERE account = :account AND datetime > datetime('now', '-30 days')",
        table_name),
        soci::use(account), soci::into(volume, ind);
    summary.volume_usd = (ind == soci::i_ok) ? volume : 0.0;
    return summary;
  }

  // ----------------------------------------------------------------------------
  volume_summary transaction_store::total_rolling_volume(
      std::chrono::system_clock::time_point now) const
  {
    std::lock_guard<pika::detail::spinlock> lock(mutex_);
    volume_summary summary;
    summary.account = "Total";
    summary.since = now - std::chrono::days(30);
    if (!sql_) { return summary; }

    double volume = 0.0;
    soci::indicator ind = soci::i_null;
    *sql_ << fmt::format("SELECT COALESCE(SUM(volume_usd), 0) FROM {} "
                         "WHERE datetime > datetime('now', '-30 days')",
        table_name),
        soci::into(volume, ind);
    summary.volume_usd = (ind == soci::i_ok) ? volume : 0.0;
    return summary;
  }

}    // namespace grox
