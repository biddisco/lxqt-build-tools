#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
//
#include "src/trade_widget.hpp"

// ----------------------------------------------------------------------------
class check_trades_dialog : public QDialog
{
    Q_OBJECT
public:
    check_trades_dialog(QWidget *parent, std::vector<trade_data> const &trades) :
        QDialog(parent)
    {
        // create a vertical layout for the parent widget
        QVBoxLayout *box = new QVBoxLayout(this);
        this->setLayout(box);
        //
        QLabel *label = new QLabel(this);
        label->setText("Please verify these trades.");
        this->layout()->addWidget(label);
        //
        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
        this->layout()->addWidget(buttonBox);
        //
        create_trade_widgets(this, trades);
    }

    static void create_trade_widgets(QWidget *parent, std::vector<trade_data> const &trades)
    {
        //
        // add a scroll area with a frame inside it
        //
        QScrollArea *orders_scrollwidget = new QScrollArea(parent);
        orders_scrollwidget->setWidgetResizable(true);
        parent->layout()->addWidget(orders_scrollwidget);
        //
        QFrame *orders_frame = new QFrame(orders_scrollwidget);
        orders_frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        orders_frame->setLayout(new QVBoxLayout());
        orders_scrollwidget->setWidget(orders_frame);
        //
        // add one widget for each trade item
        //
        for (auto const &t : trades) {
            auto widget_ = new trade_widget(orders_scrollwidget);
            widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            widget_->set_data(t);
            orders_scrollwidget->widget()->layout()->addWidget(widget_);
        }
        // absorb any extra space in the parent by adding a spacer
        orders_scrollwidget->widget()->layout()->addItem(new QSpacerItem(1,1, QSizePolicy::Minimum, QSizePolicy::Expanding));
    }
};
