// Qt
#include <QDialog>
#include <QString>
#include <QDialogButtonBox>
#include <QLineEdit>
// STL
#include <vector>
#include <string>
// Grox
#include "src/widgets/trade_algorithm.hpp"
#include "src/data/ohlc_datasets.hpp"
#include "src/stream/trade_filter.hpp"

// ----------------------------------------------------------------------------
trade_algorithm::trade_algorithm()
    : QDialog()
{
    ui.setupUi(this);
    this->setWindowTitle("Algorithm");

    // add ok, cancel buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
            | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::clicked,  this, [=](QAbstractButton* b) {
        if (buttonBox->standardButton(b) == QDialogButtonBox::Reset) {
            this->done(2);
        }
    });

    ui.buttons_layout->addWidget(buttonBox);

    // setup algorithms combobox
    for (const auto &a : available_algorithms) {
        QString s = a.name.c_str();
        ui.algorithm->addItem(s);
    }
    connect(ui.algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
        refresh_gui(index);
    } , Qt::QueuedConnection);
    refresh_gui(0);
}

// ----------------------------------------------------------------------------
trade_algorithm::~trade_algorithm()
{}

// ----------------------------------------------------------------------------
void clearLayout(QLayout* layout, bool deleteWidgets = true)
{
    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (deleteWidgets)
        {
            if (QWidget* widget = item->widget())
                widget->deleteLater();
        }
        if (QLayout* childLayout = item->layout())
            clearLayout(childLayout, deleteWidgets);
        delete item;
    }
    delete layout;
}

// ----------------------------------------------------------------------------
void trade_algorithm::refresh_gui(int index)
{
    // prepare list of datasets for combobox options
    QStringList slist;
    for (const auto &r : ohlc_chart_data::available_resolutions()) {
        slist << r.name_;
    }
    //
    auto alg = available_algorithms[index];

    QLayout *oldlayout = ui.algo_params->layout();
    if (oldlayout) clearLayout(oldlayout, true);
    combos.clear();
    params.clear();

    QGridLayout *layout = new QGridLayout;
    for (int i=0; i<alg.num_datasets; ++i) {
        QLabel * const label = new QLabel(QString("Data %1").arg(i, 2, 3, QLatin1Char('0')));
        label->setMinimumWidth(75);
        layout->addWidget(label, i, 0);
        QComboBox * const box = new QComboBox();
        box->setMinimumWidth(100);
        layout->addWidget(box, i, 1);
        combos << box;
        box->addItems(slist);
    }
    for (int i=0; i<alg.num_params; ++i) {
        QLabel * const label = new QLabel(QString("Param %1").arg(i, 2, 3, QLatin1Char('0')));
        label->setMinimumWidth(75);
        layout->addWidget(label, i, 2);
        QLineEdit * const edit = new QLineEdit();
        edit->setValidator( new QDoubleValidator(0, 100E9, 1, this) );
        edit->setMinimumWidth(100);
        edit->setText(QString::number(alg.defaults[i]));
        layout->addWidget(edit, i, 3);
        params << edit;
    }
    ui.algo_params->setLayout(layout);
    adjustSize();
    resize(minimumSizeHint());
}
