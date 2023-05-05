// Qt
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QString>
// STL
#include <string>
#include <vector>
// Grox
#include "src/data/ohlc_datasets.hpp"
#include "src/indicators/indicator_definitiions.hpp"
#include "src/stream/trade_filter.hpp"
#include "src/widgets/indicator_dialog.hpp"

// ----------------------------------------------------------------------------
indicator_dialog::indicator_dialog()
  : QDialog()
{
    ui.setupUi(this);
    this->setWindowTitle("Algorithm");

    // add ok, cancel buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::clicked, this, [=](QAbstractButton* b) {
        if (buttonBox->standardButton(b) == QDialogButtonBox::Reset)
        {
            this->done(2);
        }
    });

    ui.buttons_layout->addWidget(buttonBox);

    // setup algorithms combobox
    for (const auto& a : available_indicators)
    {
        QString s = std::visit([](const auto& obj) { return obj.name; }, a).c_str();
        ui.algorithm->addItem(s);
    }
    connect(
        ui.algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) { refresh_gui(index); }, Qt::QueuedConnection);
    refresh_gui(0);
}

// ----------------------------------------------------------------------------
indicator_dialog::~indicator_dialog() {}

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
void indicator_dialog::refresh_gui(int index)
{
    // prepare list of datasets for combobox options
    QStringList slist;
    for (const auto& r : ohlc_data_resolutions::available_resolutions())
    {
        slist << r.name_;
    }
    //
    auto alg = available_indicators[index];

    QLayout* oldlayout = ui.algo_params->layout();
    if (oldlayout)
        clearLayout(oldlayout, true);
    combos.clear();
    params.clear();

    QGridLayout* layout = new QGridLayout;
    for (int i = 0; i < std::visit([](const auto& obj) { return obj.num_datasets; }, alg);
         ++i)
    {
        const std::string& str = std::visit(
            [i](const auto& obj) { return std::get<0>(obj.datasets[i]); }, alg);
        QLabel* const label = new QLabel(QString(str.c_str()));
        label->setMinimumWidth(100);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(label, i, 0);
        QComboBox* const box = new QComboBox();
        box->setMinimumWidth(50);
        layout->addWidget(box, i, 1);
        combos << box;
        box->addItems(slist);
    }
    for (int i = 0; i < std::visit([](const auto& obj) { return obj.num_params; }, alg);
         ++i)
    {
        using namespace std::placeholders;
        auto t = std::visit([=](const auto& obj) { return obj.params[i]; }, alg);
        QLabel* const label = new QLabel(QString(std::get<0>(t).c_str()));
        label->setMinimumWidth(100);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(label, i, 2);
        QLineEdit* const edit = new QLineEdit();
        edit->setValidator(new QDoubleValidator(0, 100E9, 1, this));
        edit->setMinimumWidth(50);
        edit->setText(QString::number(std::get<1>(t)));
        layout->addWidget(edit, i, 3);
        params << edit;
    }
    ui.algo_params->setLayout(layout);
    adjustSize();
    resize(minimumSizeHint());
}
