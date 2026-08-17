#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <pika/concurrency/spinlock.hpp>
#include <soci/soci.h>

namespace grox {

  // ----------------------------------------------------------------------------
  // A single, denormalized transaction record suitable for display and analysis.
  // The schema is intentionally generic so that different exchanges can share
  // the same table.
  // ----------------------------------------------------------------------------
  struct transaction_record
  {
    std::uint64_t id = 0;
    std::string account;
    std::string datetime;
    std::int64_t timestamp_ms = 0;
    std::int32_t type = 0;
    std::optional<std::uint64_t> order_id;
    std::string market;
    std::string side;
    double amount = 0.0;
    std::string amount_ccy;
    double price = 0.0;
    std::string price_ccy;
    double fee = 0.0;
    std::string fee_ccy;
    std::optional<double> volume_usd;
    std::string raw_json;
  };

  struct volume_summary
  {
    std::string account;
    double volume_usd = 0.0;
    std::chrono::system_clock::time_point since;
  };

  // ----------------------------------------------------------------------------
  // SQLite backed store for user transactions.
  //
  // All public member functions are thread safe: a pika spinlock serialises
  // access to the underlying SOCI session because SQLite connections and SOCI
  // sessions are not thread safe.
  // ----------------------------------------------------------------------------
  class transaction_store
  {
public:
    transaction_store();
    explicit transaction_store(std::string const& db_path);
    ~transaction_store();

    transaction_store(transaction_store const&) = delete;
    transaction_store& operator=(transaction_store const&) = delete;

    void open(std::string const& db_path);
    void close();

    // Insert or update transactions from a Bitstamp /api/v2/user_transactions/
    // response.  |jdata| must be an array of transaction objects.
    void insert_bitstamp_transactions(std::string const& account, nlohmann::json const& jdata);

    // Last transaction id persisted for a given account (used for incremental
    // fetches).  Returns 0 when no rows exist.
    std::uint64_t last_transaction_id(std::string const& account) const;

    // Recent transactions, optionally filtered by account.
    std::vector<transaction_record> recent_transactions(std::size_t limit = 1000) const;
    std::vector<transaction_record> recent_transactions(
        std::string const& account, std::size_t limit = 1000) const;

    // Transactions within a datetime range (inclusive). Empty start/end means
    // unbounded. Datetimes must be in a format SQLite understands, e.g.
    // "yyyy-MM-dd HH:mm:ss".
    std::vector<transaction_record> transactions_in_range(std::string const& start_datetime,
        std::string const& end_datetime, std::size_t limit = 10000) const;
    std::vector<transaction_record> transactions_in_range(std::string const& account,
        std::string const& start_datetime, std::string const& end_datetime,
        std::size_t limit = 10000) const;

    // Rolling 30-day volume per account, plus a total across all accounts.
    // Only rows with a non-null volume_usd contribute.
    volume_summary rolling_volume(std::string const& account,
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;
    volume_summary total_rolling_volume(
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

private:
    void init_schema();
    static std::pair<std::string, std::int64_t> parse_datetime(std::string const& datetime);
    static std::optional<double> json_opt_double(nlohmann::json const& j, std::string const& key);

    mutable pika::detail::spinlock mutex_;
    std::unique_ptr<soci::session> sql_;
  };

}    // namespace grox
