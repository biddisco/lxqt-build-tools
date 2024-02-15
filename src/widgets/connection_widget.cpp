#include <QCheckBox>
#include <QRegularExpression>
//
#include <range/v3/view.hpp>
//
#include "connection_widget.hpp"
#include "ui_connection_widget.h"
#include "util/stringutils.hpp"

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
void connection_widget::setup_gui()
{
  ui->name->setText(QString::fromStdString(exchange_->name().data()));
  // -------------------------------------------
  // display available streams in a Vertical box
  QVBoxLayout* sbl = new QVBoxLayout(ui->stream_box);
  ui->stream_box->setLayout(sbl);
  //
  const auto streams = exchange_->websocket_streams();
  //
  // function to create a box with a set of stream checkboxes inside it
  auto create_stream_box = [this, sbl, streams](currency_pair cp, stream_set s) {
    QGroupBox* ticker_panel =
      new QGroupBox(QString(currency_pair_string(cp).c_str()), ui->stream_box);
    QVBoxLayout* vbox = new QVBoxLayout;
    for (auto const& s : streams)
    {
      QString txt = QString(stream_to_pretty_text(s).c_str());
      QCheckBox* bx = new QCheckBox(txt, ticker_panel);
      bx->setChecked(exchange_->is_stream_subscribed(cp, s));
      connect(
        bx, &QCheckBox::stateChanged, this,
        [this, cp, s](bool checked) { exchange_->stream_subscribe(cp, s, checked); },
        Qt::QueuedConnection);
      vbox->addWidget(bx);
    }
    ticker_panel->setLayout(vbox);
    sbl->addWidget(ticker_panel);
    sbl->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
    return ticker_panel;
  };

  // -------------------------------------------
  // display available ticker currency pairs
  model_ = new QStandardItemModel();
  filter_ = new QSortFilterProxyModel();

  enum
  {
    CheckState = Qt::UserRole + 1,
    DataState = Qt::UserRole + 2
  };

  for (auto const& [i, cp] : exchange_->get_currency_pairs() | ranges::views::enumerate)
  {
    QStandardItem* item = new QStandardItem();
    item->setText(currency_pair_qstring(cp, "/"));
    item->setCheckable(true);
    // initial state stored in user role to track checkbox changes
    if (exchange_->ticker_subscribed(std::get<0>(cp), std::get<1>(cp)))
    {
      auto* panel = create_stream_box(cp, streams);
      item->setData(Qt::Checked, CheckState);
      item->setData(QVariant::fromValue<QGroupBox*>(panel), DataState);
      item->setCheckState(Qt::Checked);
    }
    else
    {
      item->setData(Qt::Unchecked, CheckState);
      item->setData(QVariant::fromValue<QGroupBox*>(nullptr), DataState);
      item->setCheckState(Qt::Unchecked);
    }
    model_->setItem(i, item);
  }

  // attach a slot to catch item changes and update subscribed list
  connect(
    model_, &QStandardItemModel::itemChanged, this,
    [this, create_stream_box, streams](QStandardItem* item) {
      if (item->checkState() != item->data(CheckState).value<Qt::CheckState>())
      {
        // main_dbg<0>.debug(str<>("Checked changed"), item->text().toStdString(), item->checkState());
        item->setData(item->checkState(), CheckState);
        if (item->checkState() == Qt::Checked)
        {
          currency_pair cp = string_to_pair(item->text().toStdString(), "/");
          auto* panel = create_stream_box(cp, streams);
          item->setData(QVariant::fromValue<QGroupBox*>(panel), DataState);
        }
        else
        {
          auto* panel = item->data(DataState).value<QGroupBox*>();
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
