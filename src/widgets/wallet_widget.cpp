#include <utility>
//
#include "ui_wallet_widget.h"
#include "util/stringutils.hpp"
#include "widgets/currency_widget.hpp"
#include "widgets/wallet_widget.hpp"
//
// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
template <int Level>
inline constexpr print_threshold<Level, 2> wwidg_dbg("Wallet-W");

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
void wallet_widget::set_data(ledger_wallet* w)
{
  network_ = w->network_;
  account_ = w;
  int decimals = w->network_->get_decimals();
  //
  std::string wname = fmt::format("{}/{}", w->network_->get_name(), w->name_);
  wwidg_dbg<0>.debug(ffmt<s20>("set_data"), wname);
  ui->wallet_groupbox->setTitle(to_qstring(wname));
  ui->address->setText(w->public_.c_str());
  ui->address->setTextInteractionFlags(Qt::TextSelectableByMouse);
  ui->tag->setText(QString(std::to_string(w->tag_).c_str()));
  ui->tag->setTextInteractionFlags(Qt::TextSelectableByMouse);
  //
  auto l = w->lock_currencies();
  for (auto& c : w->currencies_)
  {
    wwidg_dbg<0>.debug(ffmt<s20>("assign currencies"), c.symbol_.to_stringrep());
    // widgets are added to the layout, but become children of the layout's parent
    auto* widget = ui->wallet_groupbox->findChild<currency_widget*>(c.symbol_.to_stringrep());
    if (widget == nullptr)
    {
      widget = new currency_widget(decimals, this);
      widget->setObjectName(to_qstring(c.symbol_.to_stringrep()));
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
