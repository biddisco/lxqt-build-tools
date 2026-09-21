// Trading Launcher Dialog
// Global hotkey entry point for creating trading widgets (Currency Exchange,
// Arbitrage 2-way, Market Maker). Replaces the old per-account trade-action
// buttons on the wallet_widget.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
//
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
//
#include "config/config.hpp"
#include "debug/logging.hpp"
#include "exchange/abstract_exchange.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_params.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/indicator_types.hpp"
#include "util/stringutils.hpp"
#include "widgets/trade_algorithm_widget.hpp"
#include "widgets/trading_launcher.hpp"
#include "widgets_control/indicator_widget.hpp"

#include "ui_trading_launcher.h"

namespace {

  // ------------------------------------------------------------------
  auto launcher_log = grox::log::create("Launcher");

  // ------------------------------------------------------------------
  /// Iterate the algorithm's params and return every order_book_param's
  /// tickers. Used on OK to demand-subscribe the chosen pairs.
  std::vector<currency_pair> orderbook_tickers(indicators::shared_algorithm const& alg)
  {
    std::vector<currency_pair> result;
    for (auto const& p : alg->get_params())
    {
      if (auto const* ob = std::get_if<indicators::param<order_book_param>>(&p))
      {
        for (auto const& cp : ob->get().tickers_) { result.push_back(cp); }
      }
    }
    return result;
  }

  // ------------------------------------------------------------------
  /// Find an exchange in the global settings by name. Used by the
  /// order_book_param control's set_ticker_strings lambda — we mirror it
  /// here so we can call ensure_orderbook_subscribed on the right exchange.
  std::shared_ptr<abstract_exchange> find_exchange_by_name(std::string const& name)
  {
    auto it = std::find_if(global_settings.networks_.begin(), global_settings.networks_.end(),
        [&](auto const& e) { return e->get_name() == name; });
    return (it == global_settings.networks_.end()) ? nullptr : *it;
  }

}    // namespace

// ----------------------------------------------------------------------------
trading_launcher_dialog::trading_launcher_dialog(
    abstract_exchange::exchange_vector exchanges, QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::trading_launcher())
  , exchanges_(std::move(exchanges))
{
  ui_->setupUi(this);
  setWindowTitle("New Trading Widget");

  // Style the description label as a secondary caption: slightly smaller
  // font and the palette's disabled text colour, which is theme-aware
  // (light in dark mode, grey in light mode). The intro label above
  // remains the primary instruction; the description shows the algorithm's
  // own description text as secondary context.
  QFont caption_font = ui_->description->font();
  if (caption_font.pointSize() > 0) { caption_font.setPointSize(caption_font.pointSize() - 1); }
  ui_->description->setFont(caption_font);
  QPalette caption_pal = ui_->description->palette();
  caption_pal.setColor(QPalette::Active, QPalette::WindowText,
      caption_pal.color(QPalette::Disabled, QPalette::WindowText));
  ui_->description->setPalette(caption_pal);

  // Accessibility: name each combo so screen readers announce the label
  // when focus moves to it, and add tooltips for pointer users.
  ui_->algorithm->setAccessibleName(tr("Algorithm"));
  ui_->algorithm->setToolTip(tr("Choose the trading algorithm to run."));
  ui_->exchange->setAccessibleName(tr("Exchange"));
  ui_->exchange->setToolTip(tr("Choose the exchange to run the algorithm on. "
                               "Only exchanges that support the selected "
                               "algorithm are shown."));

  // The OK button starts disabled — it is enabled only when a valid
  // exchange is selected (see repopulate_exchanges / on_exchange_changed).
  ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

  // Wire the persistent button box. On OK, copy user-edited params back
  // into the algorithm prototype via the embedded indicator_widget, then
  // accept. On Cancel, reject. On Reset, rebuild the indicator panel from
  // the unmodified algorithm prototype (restoring defaults).
  connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
    if (indicator_panel_) indicator_panel_->update_parameters();
    accept();
  });
  connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui_->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* b) {
    if (ui_->buttonBox->standardButton(b) == QDialogButtonBox::Reset) { rebuild_indicator_panel(); }
  });

  // Populate the algorithm combo from the registry's orderbook-kind
  // indicators. These are the three trading algorithms (Currency Exchange,
  // Arbitrage 2-way, Market Maker) — partitioned by kind() so we don't need
  // to hardcode names here.
  auto const& orderbook_algs =
      indicators::indicator_registry::getInstance().by_kind(indicators::indicator_kind::orderbook);
  for (auto const& alg : orderbook_algs)
  {
    ui_->algorithm->addItem(QString::fromStdString(alg->get_name()));
  }

  // When the algorithm changes, repopulate the exchange combo (filtered by
  // the algorithm's trade_action() capability) and rebuild the indicator
  // parameter panel.
  connect(ui_->algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      &trading_launcher_dialog::on_algorithm_changed);

  // When the exchange changes, rebuild the indicator parameter panel so the
  // order_book_param control picks up the new exchange.
  connect(ui_->exchange, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      &trading_launcher_dialog::on_exchange_changed);

  // Seed the dialog with the first algorithm. We call on_algorithm_changed
  // directly because addItem() leaves the combo at index 0, so
  // setCurrentIndex(0) would not emit currentIndexChanged.
  if (!orderbook_algs.empty()) { on_algorithm_changed(0); }
  else
  {
    // No trading algorithms registered — disable both combos and the OK
    // button, and show a message in the description label.
    ui_->algorithm->setEnabled(false);
    ui_->exchange->setEnabled(false);
    ui_->description->setText(
        tr("No trading algorithms are registered. Check that the trading plugin is loaded."));
  }
}

// ----------------------------------------------------------------------------
trading_launcher_dialog::~trading_launcher_dialog() { delete ui_; }

// ----------------------------------------------------------------------------
void trading_launcher_dialog::on_algorithm_changed(int index)
{
  auto const& orderbook_algs =
      indicators::indicator_registry::getInstance().by_kind(indicators::indicator_kind::orderbook);
  if (index < 0 || index >= static_cast<int>(orderbook_algs.size())) return;

  current_algorithm_ = orderbook_algs[index];
  ui_->description->setText(QString::fromStdString(current_algorithm_->get_description()));
  update_window_title();

  // Filter the exchange combo to those that support this algorithm's
  // trade_action(). If the algorithm doesn't declare a trade_action()
  // (which shouldn't happen for orderbook-kind indicators), fall back to
  // showing all exchanges.
  auto action = current_algorithm_->trade_action();
  if (!action)
  {
    GROX_LOG_WARN(launcher_log,
        "Algorithm '{}' (kind=orderbook) returned no trade_action(); "
        "showing all exchanges",
        current_algorithm_->get_name());
    repopulate_exchanges(static_cast<supported_trade_actions>(-1));
  }
  else { repopulate_exchanges(*action); }
}

// ----------------------------------------------------------------------------
void trading_launcher_dialog::on_exchange_changed(int index)
{
  if (index < 0 || index >= ui_->exchange->count())
  {
    current_exchange_.reset();
    ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    return;
  }
  current_exchange_ = find_exchange_by_name(ui_->exchange->itemText(index).toStdString());
  ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(static_cast<bool>(current_exchange_));
  update_window_title();
  rebuild_indicator_panel();
}

// ----------------------------------------------------------------------------
std::shared_ptr<abstract_exchange> trading_launcher_dialog::repopulate_exchanges(
    supported_trade_actions action)
{
  // Block signals while we refill so on_exchange_changed only fires once
  // at the end (via setCurrentIndex).
  QSignalBlocker block(ui_->exchange);
  ui_->exchange->clear();

  std::shared_ptr<abstract_exchange> first;
  for (auto const& ex : exchanges_)
  {
    // -1 means "no filter" — used when an algorithm doesn't declare a
    // trade_action(). Otherwise require the exchange to support it.
    if (action != static_cast<supported_trade_actions>(-1))
    {
      auto supported = ex->supported_trade_actions();
      if (std::find(supported.begin(), supported.end(), action) == supported.end()) continue;
    }
    ui_->exchange->addItem(QString::fromStdString(ex->get_name()));
    if (!first) first = ex;
  }

  if (ui_->exchange->count() == 0)
  {
    current_exchange_.reset();
    ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    ui_->description->setText(tr("No exchange supports this trading algorithm. "
                                 "Subscribe to an exchange first."));
    rebuild_indicator_panel();
    return nullptr;
  }

  ui_->description->setText(QString::fromStdString(current_algorithm_->get_description()));

  // Release the signal blocker. Call on_exchange_changed(0) directly
  // because addItem() already leaves the combo at index 0, so
  // setCurrentIndex(0) would not emit currentIndexChanged.
  block.unblock();
  on_exchange_changed(0);
  return first;
}

// ----------------------------------------------------------------------------
void trading_launcher_dialog::rebuild_indicator_panel()
{
  // Tear down the previous indicator_widget. It is owned by
  // indicator_panel_layout; deleting it removes it from the layout.
  if (indicator_panel_)
  {
    indicator_panel_->deleteLater();
    indicator_panel_ = nullptr;
  }

  if (!current_algorithm_ || !current_exchange_) return;

  // Count how many order_book_param params the algorithm has. Algorithms
  // with a single order_book_param (Currency Exchange, Market Maker) get
  // only the exchange chosen in our dialog's combo — the per-param
  // exchange picker is hidden by control_builder_orderbook when only one
  // exchange is provided. Algorithms with multiple order_book_params
  // (Arbitrage 2-way has Order-Book-1 and Order-Book-2) get ALL
  // exchanges that support the algorithm's trade_action() so each
  // order_book_param can pick a different exchange.
  std::size_t num_orderbook_params = 0;
  for (auto const& p : current_algorithm_->get_params())
  {
    if (std::get_if<indicators::param<order_book_param>>(&p)) ++num_orderbook_params;
  }

  nlohmann::json defaults;
  if (num_orderbook_params > 1)
  {
    // Pass all exchanges that support this algorithm's trade_action().
    // If the algorithm has no trade_action(), pass all exchanges.
    auto action = current_algorithm_->trade_action();
    std::vector<std::string> exchange_names;
    for (auto const& ex : exchanges_)
    {
      if (action)
      {
        auto supported = ex->supported_trade_actions();
        if (std::find(supported.begin(), supported.end(), *action) == supported.end()) continue;
      }
      exchange_names.push_back(ex->get_name());
    }
    defaults["exchanges"] = exchange_names;
  }
  else
  {
    // Single order_book_param: the exchange was chosen in our dialog's
    // combo above. Pass only that one so the per-param exchange picker
    // is hidden.
    defaults["exchanges"] = std::vector<std::string>{current_exchange_->get_name()};
  }
  defaults["exchange_index"] = 0;

  // The indicator_widget single-algorithm constructor builds the full
  // parameter panel (with the algorithm combo hidden) and calls
  // refresh_gui() which creates the control widgets via control_factory.
  indicator_panel_ = new indicator_widget(current_algorithm_, defaults);
  indicator_panel_->setWindowTitle(QString::fromStdString(current_algorithm_->get_name()));
  ui_->indicator_panel_layout->addWidget(indicator_panel_);

  // indicator_widget::refresh_gui() sets SetFixedSize on its parent's
  // layout (i.e. our indicator_panel_layout), which would prevent the
  // scroll area from scrolling. Override it back to the default so the
  // panel can grow beyond the scroll area's viewport and scroll.
  ui_->indicator_panel_layout->setSizeConstraint(QLayout::SetDefaultConstraint);
}

// ----------------------------------------------------------------------------
void trading_launcher_dialog::update_window_title()
{
  if (!current_algorithm_)
  {
    setWindowTitle(tr("New Trading Widget"));
    return;
  }
  if (current_exchange_)
  {
    setWindowTitle(tr("New %1 Widget — %2")
                       .arg(QString::fromStdString(current_algorithm_->get_name()))
                       .arg(QString::fromStdString(current_exchange_->get_name())));
  }
  else
  {
    setWindowTitle(tr("New %1 Widget").arg(QString::fromStdString(current_algorithm_->get_name())));
  }
}

// ----------------------------------------------------------------------------
void trading_launcher_dialog::demand_subscribe_orderbooks()
{
  if (!current_exchange_) return;
  auto tickers = orderbook_tickers(current_algorithm_);
  for (auto const& cp : tickers)
  {
    try
    {
      current_exchange_->ensure_orderbook_subscribed(cp);
    }
    catch (std::exception const& e)
    {
      GROX_LOG_ERROR(launcher_log, "ensure_orderbook_subscribed failed for {} on {}: {}",
          currency_pair_string(cp), current_exchange_->get_name(), e.what());
    }
  }
}

// ----------------------------------------------------------------------------
indicators::shared_algorithm trading_launcher_dialog::get_algorithm() const
{
  if (!current_algorithm_) return nullptr;

  // Clone the prototype (with user-edited params) so the registry's
  // prototype is not mutated. trade_widget_factory takes a shared_algorithm
  // and produces the docked widget.
  auto alg_copy = std::static_pointer_cast<indicators::algorithm_base>(current_algorithm_->clone());

  // Demand-subscribe the order book streams the algorithm needs on the
  // chosen exchange. This must happen before trade_widget_factory builds
  // the GUI, because gui_trade_* calls get_subscribed_ticker_data() which
  // throws if the ticker isn't subscribed.
  //
  // We do this here (in the getter, after the dialog is accepted) rather
  // than in rebuild_indicator_panel() because the user's final ticker
  // selection is only known after OK.
  const_cast<trading_launcher_dialog*>(this)->demand_subscribe_orderbooks();

  return alg_copy;
}
