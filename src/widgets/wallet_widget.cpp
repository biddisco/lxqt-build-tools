#include <utility>
//
#include <QPushButton>
//
#include "ui_wallet_widget.h"
#include "util/stringutils.hpp"
#include "widgets/currency_widget.hpp"
#include "widgets/wallet_widget.hpp"
//
#include "debug/logging.hpp"
//
// ----------------------------------------------------------------------------
static auto wwidg_log = grox::log::create("Wallet-W");

// ----------------------------------------------------------------------------
wallet_widget::wallet_widget(QWidget* parent)
  : QWidget(parent)
  , ui(new Ui::wallet_widget)
{
  ui->setupUi(this);
  //
  connect(
      ui->net_funcs, &QToolButton::clicked, this,
      [this](bool /*checked*/) { network_->custom_functions(account_); }, Qt::QueuedConnection);
}

// ----------------------------------------------------------------------------
wallet_widget::~wallet_widget() { delete ui; }

// ----------------------------------------------------------------------------
void wallet_widget::add_algorithm(std::string name, std::function<void(void)> f)
{
  int col = ui->algorithms_layout->count() % 3;
  int row = ui->algorithms_layout->count() / 3;
  auto* button = new QPushButton(to_qstring(name), this);
  ui->algorithms_layout->addWidget(button, row, col);
  connect(
      button, &QToolButton::clicked, this, [f](bool /*checked*/) { f(); }, Qt::QueuedConnection);
}

// ----------------------------------------------------------------------------
void wallet_widget::set_data(ledger_wallet* w)
{
  network_ = w->network_;
  account_ = w;
  int decimals = w->network_->get_decimals();
  //
  std::string wname = fmt::format("{}/{}", w->network_->get_name(), w->name_);
  GROX_LOG_DEBUG(wwidg_log, "{:>20} {}", "set_data", wname);
  ui->wallet_groupbox->setTitle(to_qstring(wname));
  ui->address->setText(w->public_.c_str());
  ui->address->setTextInteractionFlags(Qt::TextSelectableByMouse);
  ui->tag->setText(QString(std::to_string(w->tag_).c_str()));
  ui->tag->setTextInteractionFlags(Qt::TextSelectableByMouse);
  //
  auto l = w->lock_currencies();
  for (auto& c : w->currencies_)
  {
    GROX_LOG_DEBUG(wwidg_log, "{:>20} {}", "assign currencies", c.symbol_.to_stringrep(true, true));
    // widgets are added to the layout, but become children of the layout's parent
    auto* widget =
        ui->wallet_groupbox->findChild<currency_widget*>(c.symbol_.to_stringrep(true, true));
    if (widget == nullptr)
    {
      widget = new currency_widget(decimals, this);
      widget->setObjectName(to_qstring(c.symbol_.to_stringrep(true, true)));
      widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      ui->currencies_layout->addWidget(widget);
    }
    widget->set_data(&c, w, w->network_);
  }
  w->unlock_currencies(std::move(l));
  //
  update();
}

// ----------------------------------------------------------------------------
