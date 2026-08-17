#!/ usr / bin / env python
#coding : utf - 8
"""Simple helper to inspect the grox SQLite transaction store."""

import argparse
import os
import sqlite3
import sys

import pandas as pd
from PySide6.QtCore import QStandardPaths


def get_default_db_path():
    locations = QStandardPaths.standardLocations(QStandardPaths.StandardLocation.AppDataLocation)
    base = locations[0] if locations else os.path.expanduser("~/.local/share")
    return os.path.join(base, "grox", "grox_transactions.db")


def main():
    parser = argparse.ArgumentParser(
        description="Display recent transactions from the grox SQLite store."
    )
    parser.add_argument(
        "--db", default=get_default_db_path(), help="Path to grox_transactions.db"
    )
    parser.add_argument("--account", default=None, help="Filter by account name")
    parser.add_argument("--market", default=None, help="Filter by market/pair (e.g. XRP/EUR)")
    parser.add_argument("--limit", type=int, default=100, help="Maximum rows to display")
    parser.add_argument("--volume_days", type=int, default=30, help="Rolling volume window in days")
    args = parser.parse_args()

    if not os.path.exists(args.db):
        print(f"Database not found: {args.db}")
        sys.exit(1)

    print(f"Using database: {args.db}")

    conn = sqlite3.connect(args.db)

    query = "SELECT * FROM bitstamp_user_transactions WHERE 1=1"
    params = {}
    if args.account:
        query += " AND account = :account"
        params["account"] = args.account
    if args.market:
        query += " AND market = :market"
        params["market"] = args.market
    query += " ORDER BY datetime DESC LIMIT :limit"
    params["limit"] = args.limit

    df = pd.read_sql_query(query, conn, params=params)
    if df.empty:
        print("No transactions found.")
        return

#Convert integer timestamp back to readable datetime if raw column missing
    if "datetime" not in df.columns and "timestamp_ms" in df.columns:
        df["datetime"] = pd.to_datetime(df["timestamp_ms"], unit="ms", utc=True)

    cols = [
        "id",
        "order_id",
        "datetime",
        "account",
        "type",
        "market",
        "side",
        "amount",
        "amount_ccy",
        "price",
        "price_ccy",
        "fee",
        "fee_ccy",
        "volume_usd",
    ]
    display_cols = [c for c in cols if c in df.columns]
    print(df[display_cols].to_string(index=False))

#rolling volume summary
    vol_query = """
        SELECT account, COALESCE(SUM(volume_usd), 0) AS volume_usd
        FROM bitstamp_user_transactions
        WHERE datetime > datetime('now', '-{days} days')
    """.format(days=args.volume_days)
    if args.account:
        vol_query += " AND account = :account"
    vol_query += " GROUP BY account ORDER BY account"

    vol_df = pd.read_sql_query(vol_query, conn, params={
  "account" : args.account} if args.account else {})
    if not vol_df.empty:
        print("\nRolling {}-day volume per account (USD, where convertible):".format(args.volume_days))
        print(vol_df.to_string(index=False))
        total = vol_df["volume_usd"].sum()
        print(f"Total: {total:,.2f} USD")


if __name__ == "__main__":
    main()
