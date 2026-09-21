#pragma once

#include <cstddef>
//
#include <QDialog>
#include <QString>
//
#include "exchange/abstract_exchange.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_registry.hpp"

namespace Ui {
  class trading_launcher;
}    // namespace Ui

// ----------------------------------------------------------------------------
/// Dialog that lets the user pick a trading algorithm (Currency Exchange,
/// Arbitrage 2-way, Market Maker) and the exchange to run it on, then
/// configure the algorithm's parameters (currency pairs etc.) before a
/// trading widget is created.
///
/// Driven by a global hotkey (Ctrl+T) in mainwindow.cpp. Replaces the old
/// per-account trade-action buttons on the wallet_widget.
///
/// The exchange combo is filtered by each algorithm's trade_action() so only
/// exchanges that support that action are offered. The algorithm parameter
/// panel is rendered by a nested indicator_widget (single-algorithm
/// constructor) so all existing control_builder machinery — including the
/// order_book_param's currency pair picker — is reused.
class trading_launcher_dialog : public QDialog
{
  Q_OBJECT

  public:
  explicit trading_launcher_dialog(
      abstract_exchange::exchange_vector exchanges, QWidget* parent = nullptr);
  ~trading_launcher_dialog() override;

  /// The algorithm prototype selected by the user, with all GUI-edited
  /// parameters applied. Valid only after the dialog is Accepted.
  /// Returns nullptr if no valid selection was made.
  indicators::shared_algorithm get_algorithm() const;

  private slots:
  void on_algorithm_changed(int index);
  void on_exchange_changed(int index);

  private:
  /// Filter the exchange combo to those that support the current algorithm's
  /// trade_action(). Returns the exchange that should be selected (the first
  /// one), or nullptr if none.
  std::shared_ptr<abstract_exchange> repopulate_exchanges(supported_trade_actions action);

  /// Tear down the previous indicator_widget and create a fresh one for the
  /// current algorithm, seeding it with the chosen exchange so the
  /// order_book_param control picks it up. The nested indicator_widget's
  /// own OK/Cancel/Reset buttons (added via add_indicator_to_dialog) are
  /// wired to accept/reject this dialog and call update_parameters() on OK.
  void rebuild_indicator_panel();

  /// For each currency pair in the algorithm's order_book_param, demand-
  /// subscribe the order book stream on the chosen exchange so the trading
  /// widget has data when it is created.
  void demand_subscribe_orderbooks();

  /// Update the dialog window title to reflect the current algorithm and
  /// exchange selection (e.g. "New Market-Maker Widget — Bitstamp").
  void update_window_title();

  Ui::trading_launcher* ui_;

  /// Full list of exchanges, captured by value. The exchange combo shows a
  /// filtered subset based on the current algorithm's trade_action().
  abstract_exchange::exchange_vector exchanges_;

  /// The algorithm prototype currently selected (a copy from the registry).
  /// Cloned again on OK so user-edited params persist back into the registry
  /// prototype (the indicator_widget::update_parameters() path).
  indicators::shared_algorithm current_algorithm_;

  /// The exchange currently selected in the combo, or nullptr if the combo
  /// is empty.
  std::shared_ptr<abstract_exchange> current_exchange_;

  /// The nested indicator_widget that renders the algorithm parameter
  /// panel. Owned by indicator_panel_layout; recreated on every
  /// algorithm/exchange change.
  class indicator_widget* indicator_panel_{nullptr};
};
