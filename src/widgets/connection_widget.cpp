#include <QCheckBox>
#include <QRegularExpression>
//
#include "connection_widget.hpp"
#include "ui_connection_widget.h"
//
#include "range/v3/view.hpp"

// ----------------------------------------------------------------------------
connection_widget::connection_widget(QWidget *parent, net::contexts &io_contexts, exchange *ex) :
    QWidget(parent)
  , ui(new Ui::connection_widget)
  , io_contexts_(io_contexts)
  , exchange_(ex)
{
    ui->setupUi(this);
    connect(ui->filter, SIGNAL(textChanged(const QString &)), this, SLOT(filter_changed(const QString &)));
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
    for (const auto &s : streams) {
        QString name = QString(stream_text(s).c_str());
        QCheckBox *bx = new QCheckBox(name, ui->stream_box);
        bx->setChecked(exchange_->websocket_enabled(s));
        connect(bx, &QCheckBox::stateChanged, this, [this, s](bool checked) {
            exchange_->websocket_enable(s, io_contexts_, checked);
        } , Qt::QueuedConnection);

        sbl->addWidget(bx);
    }
    sbl->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
    ui->stream_box->setLayout(sbl);

    // -------------------------------------------
    // display available ticker currency pairs
    model_ = new QStandardItemModel();
    filter_ = new QSortFilterProxyModel();

    enum {CheckState = Qt::UserRole + 1};
    for (auto const& [i, cp] : exchange_->currency_pairs() | ranges::views::enumerate) {
        QStandardItem *item = new QStandardItem();
        item->setText(cp.first.curr_.code_.c_str() + QString("/") + cp.second.curr_.code_.c_str());
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked);
        // initial state stored in user role to track checkbox changes
        item->setData(Qt::Unchecked, CheckState);
        model_->setItem(i, item);
    }
    // attach a slot to catch item changes and update subscribed list
    connect(model_, &QStandardItemModel::itemChanged, this, [this](QStandardItem* item) {
        if (item->checkState() != item->data(CheckState).value<Qt::CheckState>()) {
            // main_dbg<0>.debug(str<>("Checked changed"), item->text().toStdString(), item->checkState());
            item->setData(item->checkState(), CheckState);
            auto *sl = ui->subscribed_list;
            if (item->checkState() == Qt::Checked) {
                sl->addItem(item->text());
            }
            else {
                auto items = sl->findItems(item->text(), Qt::MatchFlag::MatchCaseSensitive);
                for (auto *item : items) {
                    delete sl->takeItem(sl->row(item));
                }
            }
        }
    }, Qt::QueuedConnection);

    filter_->setSourceModel(model_);
    ui->tickers_list->setModel(filter_);
}

// ----------------------------------------------------------------------------
void connection_widget::filter_changed(const QString &s)
{
    QRegularExpression::PatternOption po = QRegularExpression::PatternOption::CaseInsensitiveOption;
    QRegularExpression regExp(s, po);
    filter_->setFilterRegularExpression(regExp);
}

// ----------------------------------------------------------------------------
void connection_widget::apply()
{
    auto *sl = ui->subscribed_list;
    for(int i = 0; i < sl->count(); ++i)
    {
        exchange_->subscribe_currency_pair(sl->item(i)->text().toStdString());
    }
}


