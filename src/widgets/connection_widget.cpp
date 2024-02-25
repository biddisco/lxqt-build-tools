#include <QCheckBox>
#include <QRegularExpression>
//
#include <range/v3/view.hpp>
//
#include "debug/print.hpp"
#include "util/stringutils.hpp"
#include "widgets/collapsible_groupbox.hpp"
#include "widgets/connection_widget.hpp"
//
#include "ui_connection_widget.h"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
template <int Level>
static print_threshold<Level, 2> conn_dbg("CxWidget");

// ----------------------------------------------------------------------------
connection_widget::connection_widget(QWidget* parent, exchange* ex)
  : QWidget(parent)
  , ui(new Ui::connection_widget)
  , exchange_(ex)
{
  ui->setupUi(this);
  connect(
    ui->filter, SIGNAL(textChanged(QString const&)), this, SLOT(filter_changed(QString const&)));
}

// ----------------------------------------------------------------------------
connection_widget::~connection_widget()
{
  delete ui;
  delete filter_;
  delete model_;
}

// ----------------------------------------------------------------------------
enum
{
  CheckState = Qt::UserRole + 0,
  DataState = Qt::UserRole + 1
};

// ----------------------------------------------------------------------------
void connection_widget::setup_gui()
{
  ui->name->setText(QString::fromStdString(exchange_->get_name()));
  // -------------------------------------------
  // display available streams in a Vertical box
  QVBoxLayout* sbl = new QVBoxLayout(ui->stream_box);
  ui->stream_box->setLayout(sbl);
  //
  const auto streams = exchange_->websocket_streams();

  // function to create a box with a set of stream checkboxes inside it
  auto create_stream_box = [this, sbl, streams](currency_pair cp, stream_set s) {
    CollapsibleGroupBox* ticker_panel =
      new CollapsibleGroupBox(currency_pair_qstring(cp), ui->stream_box);
    QVBoxLayout* vbox = new QVBoxLayout;
    for (auto const& s : streams)
    {
      QString txt = QString(stream_to_pretty_text(s).c_str());
      QCheckBox* bx = new QCheckBox(txt, ticker_panel);
      bx->setChecked(exchange_->is_stream_subscribed(cp, s));
      connect(
        bx, &QCheckBox::stateChanged, this,
        [this, cp, s](bool checked) {    //
          exchange_->stream_subscribe(cp, s, checked, exchange_->get_factory("stream_subscribe"));
        },
        Qt::QueuedConnection);
      vbox->addWidget(bx);
    }
    ticker_panel->setLayout(vbox);
    sbl->addWidget(ticker_panel);
    sbl->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
    return ticker_panel;
  };

  // create box and also populate model
  auto create_stream_panel = [create_stream_box](currency_pair cp, stream_set streams,
                               QStandardItem* item, bool checked) {
    // state stored in user role to track checkbox changes
    if (checked)
    {
      auto* panel = create_stream_box(cp, streams);
      item->setData(QVariant::fromValue<CollapsibleGroupBox*>(panel), DataState);
      item->setData(Qt::Checked, CheckState);
      item->setCheckState(Qt::Checked);
    }
    else
    {
      item->setData(QVariant::fromValue<CollapsibleGroupBox*>(nullptr), DataState);
      item->setData(Qt::Unchecked, CheckState);
      item->setCheckState(Qt::Unchecked);
    }
    return item;
  };

  // -------------------------------------------
  // display available ticker currency pairs
  model_ = new QStandardItemModel();
  filter_ = new QSortFilterProxyModel();

  // on initial creation, add all tickers to the model
  for (auto const& [i, cp] : exchange_->get_currency_pairs() | ranges::views::enumerate)
  {
    QStandardItem* item = new QStandardItem();
    item->setText(currency_pair_qstring(cp, "/"));
    item->setCheckable(true);
    bool checked = exchange_->ticker_subscribed(std::get<0>(cp), std::get<1>(cp));
    create_stream_panel(cp, streams, item, checked);
    model_->setItem(i, item);
  }

  // attach a slot to catch item changes and update subscribed list
  // caution : itemChanged is not triggered only when the checkstate changes
  // so we compare the checkstate to the stored state before making changes
  connect(
    model_, &QStandardItemModel::itemChanged, this,
    [this, create_stream_panel, streams](QStandardItem* item) {
      if (item->checkState() != item->data(CheckState).value<Qt::CheckState>())
      {
        item->setData(item->checkState(), CheckState);
        if (item->checkState() == Qt::Checked)
        {
          currency_pair cp = string_to_pair(item->text().toStdString(), "/");
          ticker_data empty;
          exchange_->get_factory("ticker_subscribe")(cp, empty, {});
          create_stream_panel(cp, streams, item, true);
        }
        else
        {
          // the streams are being unsubscribed from
          auto* panel = item->data(DataState).value<CollapsibleGroupBox*>();
          for (auto s : streams)
          {
            auto stream = std::string(magic_enum::enum_name(s));
            currency_pair cp = string_to_pair(item->text().toStdString(), "/");
            conn_dbg<0>.debug(str<>("Unsubscribe"), item->text().toStdString(), stream);
            exchange_->ticker_unsubscribe(std::get<0>(cp), std::get<1>(cp));
          }
          delete panel;
        }
      }
    },
    Qt::QueuedConnection);

  filter_->setSourceModel(model_);
  ui->tickers_list->setModel(filter_);
}

// ----------------------------------------------------------------------------
void connection_widget::filter_changed(QString const& s)
{
  QRegularExpression::PatternOption po = QRegularExpression::PatternOption::CaseInsensitiveOption;
  QRegularExpression regExp(s, po);
  filter_->setFilterRegularExpression(regExp);
}
