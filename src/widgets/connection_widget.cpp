#include <QCheckBox>
#include <QRegularExpression>
#include <QStandardItemModel>
//
#include <range/v3/view.hpp>
//
#include "config/config.hpp"
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
  DataState = Qt::UserRole + 1,
  StreamState = Qt::UserRole + 2
};

// ----------------------------------------------------------------------------
void connection_widget::setup_gui()
{
  // setup model/view/filter for gui control
  model_ = new QStandardItemModel();
  filter_ = new QSortFilterProxyModel();
  filter_->setAutoAcceptChildRows(true);
  ui->name->setText(QString::fromStdString(exchange_->get_name()));

  // on creation, add all tickers available on the exchange to the model + gui
  for (auto const& [i, cp] : exchange_->get_currency_pairs() | ranges::views::enumerate)
  {
    // create an item for each ticker, unchecked to go into the listbox gui
    QStandardItem* ticker_item = new QStandardItem();
    ticker_item->setCheckable(true);
    ticker_item->setData(currency_pair_qstring(cp, "-"), Qt::DisplayRole);
    ticker_item->setData(QVariant::fromValue<CollapsibleGroupBox*>(nullptr), DataState);
    ticker_item->setData(Qt::Unchecked, CheckState);
    ticker_item->setData(-1, StreamState);
    ticker_item->setCheckState(Qt::Unchecked);
    model_->setItem(i, ticker_item);
    stream_set streams_avail = exchange_->websocket_streams();
    QList<QStandardItem*> children;
    for (auto s : streams_avail)
    {
      QStandardItem* stream_item = new QStandardItem();
      stream_item->setCheckable(true);
      stream_item->setData(QString(stream_to_pretty_text(s).c_str()), Qt::DisplayRole);
      stream_item->setData(Qt::Unchecked, CheckState);
      stream_item->setData(s, StreamState);
      stream_item->setCheckState(Qt::Unchecked);
      children.append(stream_item);
    }
    ticker_item->appendColumn(children);
  }

  // attach a slot to catch item changes and update subscribed list
  // caution : itemChanged is not triggered _only_ when the checkstate changes
  // so we compare the checkstate to the stored state before making changes
  connect(
    model_, &QStandardItemModel::itemChanged, this,
    [this](QStandardItem* item) {
      if (item->checkState() != item->data(CheckState).value<Qt::CheckState>())
      {
        item->setData(item->checkState(), CheckState);
        auto stream = magic_enum::enum_cast<network::streams>(item->data(StreamState).toInt());
        bool ticker_node = !stream.has_value();
        currency_pair cp = (ticker_node) ?
          string_to_pair(item->text().toStdString(), "-") :
          string_to_pair(item->parent()->text().toStdString(), "-");
        //
        if (item->checkState() == Qt::Checked)
        {
          if (ticker_node)
            exchange_->ticker_subscribe(cp);
          else
          {
            exchange_->stream_subscribe(
              cp, stream.value(), true, exchange_->get_factory("stream_subscribe"));
            //item->parent()->setData(item->checkState(), CheckState);
          }
        }
        else
        {
          if (ticker_node)    // ticker node deselected, uncheck all streams
          {
            for (int i = 0; i < item->rowCount(); ++i)
            {
              QStandardItem* child = item->child(i);
              child->setCheckState(Qt::Unchecked);
            }
          }
          else
          {
            exchange_->stream_subscribe(
              cp, stream.value(), false, exchange_->get_factory("stream_unsubscribe"));
          }
        }
      }
    },
    Qt::QueuedConnection);

  // open ini file and get the group for the exchange tickers
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);
  // open global settings streams section
  settings.beginGroup("Streams");
  // open group for this exchange
  settings.beginGroup(QString::fromStdString(exchange_->get_name()));
  // get all subscribed tickers on this exchange from ini file
  QStringList children = settings.childGroups();
  for (const auto& ticker : children)
  {
    QList<QStandardItem*> list = model_->findItems(ticker, Qt::MatchExactly);
    // list length should never be >1
    for (QStandardItem* item : list)
    {
      item->setCheckState(Qt::Checked);
      settings.beginGroup(ticker);
      for (auto const& k : settings.childKeys())
      {
        bool subscribed = settings.value(k).toBool();
        if (subscribed)
        {
          network::streams stream =
            magic_enum::enum_cast<network::streams>(k.toLatin1().toStdString()).value();
          std::string txt = stream_to_pretty_text(stream);
          std::cout << k.toLatin1().data() << " " << subscribed << " " << txt << std::endl;

          QStandardItem* child = item->child(magic_enum::enum_integer(stream), 0);
          child->setCheckState(Qt::Checked);
        }
      }
      settings.endGroup();
    }
  }

  /*
  // is this ticker subscribed to
  bool ticker_subscribed = (children.contains(currency_pair_qstring(cp)));
  if (ticker_subscribed)
  {
    stream_set streams_avail = exchange_->ticker_subscribe(cp);

    if (checked)
    {
      stream_set streams_avail = exchange_->ticker_subscribe(cp);
      auto* panel = create_checkbox_per_stream(cp, streams);
      item->setData(QVariant::fromValue<CollapsibleGroupBox*>(panel), DataState);
      item->setData(Qt::Checked, CheckState);
      item->setCheckState(Qt::Checked);
    }
    else
    {
      // sub groups are tickers on the exchange
      QStringList children = settings.childGroups();
      for (const auto& ticker : children)
      {
        std::string cps = ticker.toStdString();
        currency_pair cp = string_to_pair(cps, "-");
        // subscribe to this ticker and get the streams available back
        stream_set streams_avail = exchange_->ticker_subscribe(cp);
      }
      settings.endGroup();    // exchange

      settings.endGroup();    // streams

      // -------------------------------------------
      //
      const auto streams = exchange_->websocket_streams();

*/
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
