#include "connection_widget.hpp"
#include "ui_connection_widget.h"
#include <QCheckBox>
#include <QStandardItemModel>

//connection_widget::connection_widget(QWidget *parent) :
//    QWidget(parent),
//    ui(new Ui::connection_widget)
//{
//    ui->setupUi(this);
//    connect_events();
//}

connection_widget::connection_widget(net::contexts &io_contexts, QWidget *parent) :
    QWidget(parent)
  , ui(new Ui::connection_widget)
  , io_contexts_(io_contexts)
{
    ui->setupUi(this);
//    net_layout_ = new QVBoxLayout();
    // ui->network_box->setLayout(net_layout_);
}

connection_widget::~connection_widget()
{
    delete ui;
}

void connection_widget::update_exchange_data(exchange *ex)
{
    connect_events();
}

void connection_widget::connect_events()
{
}

QGroupBox *connection_widget::get_stream_box() {
    return ui->stream_box;
}

QListView *connection_widget::get_tickers_list() {
    return ui->tickers_list;
}

QListWidget *connection_widget::get_subscribed_list() {
    return ui->subscribed_list;
}

