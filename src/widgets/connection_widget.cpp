#include "connection_widget.hpp"
#include "ui_connection_widget.h"
#include <QCheckBox>

connection_widget::connection_widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::connection_widget)
{
    ui->setupUi(this);
    connect_events();
}

connection_widget::connection_widget(net::contexts &io_contexts, const exchange::exchange_vector &exchanges, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::connection_widget)
{
    ui->setupUi(this);
    QVBoxLayout* layout = new QVBoxLayout();
    //
    for (const auto &ex : exchanges) {
        QGroupBox *gb = new QGroupBox(QString::fromStdString(ex->name().data()), this);
        QVBoxLayout* bl = new QVBoxLayout(gb);
        const auto streams = ex->websocket_streams();
        for (const auto &s : streams) {
            QString name = QString(stream_text(s).c_str());
            QCheckBox *bx = new QCheckBox(name, this);
            bx->setChecked(ex->websocket_enabled(s));
            connect(bx, &QCheckBox::stateChanged, this, [s, &io_contexts, &ex](bool checked) {
                ex->websocket_enable(s, io_contexts, checked);
            } , Qt::QueuedConnection);

            bl->addWidget(bx);
        }
        gb->setLayout(bl);
        layout->addWidget(gb);
    }
    ui->stream_box->setLayout(layout);
    connect_events();
}

connection_widget::~connection_widget()
{
    delete ui;
}

void connection_widget::connect_events()
{
}
