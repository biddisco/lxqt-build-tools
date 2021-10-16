#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
//
#include "src/widgets/trade_widget.hpp"

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

        // add a scroll area with a frame to hold widgets
        QScrollArea *orders_scrollwidget = new QScrollArea(this);
        orders_scrollwidget->setWidgetResizable(true);
        //
        QFrame *main_frame = new QFrame(this);
        main_frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        main_frame->setLayout(new QVBoxLayout());
        orders_scrollwidget->setWidget(main_frame);
        this->layout()->addWidget(orders_scrollwidget);

        // create all the widgets
        create_trade_widgets(main_frame, "Please check", trades);

        // Add ok and cancel buttons
        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
        this->layout()->addWidget(buttonBox);
    }

    static void create_trade_widgets(QWidget *main_frame, std::string const &name, std::vector<trade_data> const &trades)
    {
        // Delete the old order widgets for this network
        QWidget *old_frame = main_frame->findChild<QWidget*>(QString(name.c_str()));
        if (old_frame) delete old_frame;

        // no need to create anything new if trades are empty
        if (trades.size()==0)
            return;

        QFrame *orders_frame = new QFrame(main_frame);
        orders_frame->setObjectName(QString(name.c_str()));
        orders_frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        orders_frame->setLayout(new QVBoxLayout());

        // Add a label to say which network these orders are for
        QLabel *label = new QLabel(orders_frame);
        label->setText(QString("Orders : ") + name.c_str());
        orders_frame->layout()->addWidget(label);

        //
        // add one widget for each trade item
        //
        for (auto const &t : trades) {
            auto widget_ = new trade_widget(nullptr);
            widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            widget_->set_data(t);
            orders_frame->layout()->addWidget(widget_);
        }
        main_frame->layout()->addWidget(orders_frame);
    }
};
