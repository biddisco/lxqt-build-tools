#include <QCheckBox>
#include <QRegularExpression>
//
#include <range/v3/view.hpp>
//
#include "connection_widget.hpp"
#include "ui_connection_widget.h"

// ----------------------------------------------------------------------------
connection_widget::connection_widget(QWidget* parent, exchange* ex)
  : QWidget(parent)
  , ui(new Ui::connection_widget)
  , exchange_(ex)
{
  ui->setupUi(this);
  connect(
    ui->filter, SIGNAL(textChanged(QString const&)), this, SLOT(filter_changed(QString const&)));
  connect(ui->apply, SIGNAL(clicked()), this, SLOT(apply()));
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
  const auto streams = exchange_->websocket_streams();
  // for each ticker we are subscribed to
  for (auto const& t : exchange_->tickers_subscribed())
  {
    for (auto const& s : streams)
    {
      std::string key = currency_pair_string(t.first) + "/" + stream_to_text(s);
      QCheckBox* bx = new QCheckBox(QString(key.c_str()), ui->stream_box);
      bx->setChecked(exchange_->stream_subscribed(key));
      connect(
        bx, &QCheckBox::stateChanged, this,
        [this, t, s](bool checked) {
          //
          exchange_->stream_subscribe(t.first, s, checked);
        },
        Qt::QueuedConnection);

      sbl->addWidget(bx);
    }
  }
  sbl->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
  ui->stream_box->setLayout(sbl);

  // -------------------------------------------
  // display available ticker currency pairs
  model_ = new QStandardItemModel();
  filter_ = new QSortFilterProxyModel();

  enum
  {
    CheckState = Qt::UserRole + 1
  };
  for (auto const& [i, cp] : exchange_->get_currency_pairs() | ranges::views::enumerate)
  {
    QStandardItem* item = new QStandardItem();
    item->setText(
      std::get<0>(cp).curr_.code_.c_str() + QString("/") + std::get<1>(cp).curr_.code_.c_str());
    item->setCheckable(true);
    // initial state stored in user role to track checkbox changes
    if (exchange_->ticker_subscribed(std::get<0>(cp), std::get<1>(cp)))
    {
      item->setData(Qt::Checked, CheckState);
      item->setCheckState(Qt::Checked);
      ui->subscribed_list->addItem(item->text());
    }
    else
    {
      item->setData(Qt::Unchecked, CheckState);
      item->setCheckState(Qt::Unchecked);
    }
    model_->setItem(i, item);
  }
  // attach a slot to catch item changes and update subscribed list
  connect(
    model_, &QStandardItemModel::itemChanged, this,
    [this](QStandardItem* item) {
      if (item->checkState() != item->data(CheckState).value<Qt::CheckState>())
      {
        // main_dbg<0>.debug(str<>("Checked changed"), item->text().toStdString(), item->checkState());
        item->setData(item->checkState(), CheckState);
        auto* sl = ui->subscribed_list;
        if (item->checkState() == Qt::Checked)
        {
          sl->addItem(item->text());
        }
        else
        {
          auto items = sl->findItems(item->text(), Qt::MatchFlag::MatchCaseSensitive);
          for (auto* item : items)
          {
            delete sl->takeItem(sl->row(item));
          }
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

// ----------------------------------------------------------------------------
void connection_widget::apply()
{
  // add any new subscribed tickers to exchange list
  auto* sl = ui->subscribed_list;
  for (int i = 0; i < sl->count(); ++i)
  {
    std::string s = sl->item(i)->text().toStdString();
    auto temp = s.find("/");
    std::string c1 = s.substr(0, temp);
    std::string c2 = s.substr(temp + 1, s.back());
    exchange_->ticker_subscribe(c1, c2);
  }
  // remove any unsubscribed ones
  auto ticker_copy = exchange_->tickers_subscribed();
  for (auto& [ticker, data] : ticker_copy)
  {
    std::string text = currency_pair_string(ticker, "/");
    auto list = sl->findItems(QString(text.c_str()), Qt::MatchFlag::MatchExactly);
    if (list.empty())
    {
      exchange_->ticker_unsubscribe(std::get<0>(ticker), std::get<1>(ticker));
    }
  }
}
