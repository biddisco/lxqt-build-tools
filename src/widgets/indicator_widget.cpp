// STL
#include <string>
#include <vector>
// Qt
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
// Grox
#include "data/ohlc_dataset.hpp"
#include "indicators/indicator_params.hpp"
#include "widgets/indicator_controls.hpp"
#include "widgets/indicator_widget.hpp"

// ----------------------------------------------------------------------------
indicator_widget::indicator_widget(indicators::indicator_vector const& i, std::size_t& index)
  : QWidget()
  , indicators_(i)
  , index_(index)
{
  ui.setupUi(this);
  this->setWindowTitle("Indicator");

  // setup algorithms combobox
  for (auto const& a : indicators_)
  {
    QString s = a->get_name().c_str();
    // std::visit([](auto const& obj) { return obj.get_name(); }, a).c_str();
    ui.algorithm->addItem(s);
  }
  // when algorithm is changed, rebuild gui
  connect(
      ui.algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      [this](int index) {
        index_ = index;
        refresh_gui(index);
      },
      Qt::QueuedConnection);

  // build gui for first/last used algorithm
  ui.algorithm->setCurrentIndex(index_);
  refresh_gui(index_);
}

// ----------------------------------------------------------------------------
indicator_widget::~indicator_widget() {}

// ----------------------------------------------------------------------------
// return a variant containing a copy of the selected algorithm
// including all parameters set by the user in the dialog
indicators::algorithm_ptr indicator_widget::get_algorithm()
{
  int index = ui.algorithm->currentIndex();
  return indicators_[index];
}

// ----------------------------------------------------------------------------
void indicator_widget::add_to_dialog(QDialog* dlg)
{
  // add ok, cancel buttons
  QDialogButtonBox* buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
  connect(buttonBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
  connect(buttonBox, &QDialogButtonBox::clicked, this, [=](QAbstractButton* b) {
    if (buttonBox->standardButton(b) == QDialogButtonBox::Reset)
    {    //
      dlg->done(2);
    }
    else if (buttonBox->standardButton(b) == QDialogButtonBox::Ok)
    {
      update_parameters();
      dlg->accept();
    }
    else if (buttonBox->standardButton(b) == QDialogButtonBox::Cancel)
    {    //
      dlg->reject();
    }
  });
  ui.buttons_layout->addWidget(buttonBox);
  //
  QVBoxLayout* VLayout = new QVBoxLayout(dlg);
  VLayout->addWidget(this);
  dlg->setLayout(VLayout);
}

// ----------------------------------------------------------------------------
void clearLayout(QLayout* layout, bool deleteWidgets = true)
{
  while (QLayoutItem* item = layout->takeAt(0))
  {
    if (deleteWidgets)
    {
      if (QWidget* widget = item->widget()) widget->deleteLater();
    }
    if (QLayout* childLayout = item->layout()) clearLayout(childLayout, deleteWidgets);
    delete item;
  }
  delete layout;
}

// ----------------------------------------------------------------------------
template <typename P>
int get_column(P const& param)
{
  return 1;
}

template <>
int get_column(candle_res const& param)
{
  return 0;
}

// ----------------------------------------------------------------------------
void indicator_widget::refresh_gui(int index)
{
  // wipe the contents of the dialog
  QLayout* oldlayout = ui.algo_params->layout();
  if (oldlayout) clearLayout(oldlayout, true);
  params_.clear();
  std::array<int, 2> counts = {0, 0};

  auto alg = indicators_[index];
  std::string desc = alg->get_description();
  // std::visit([](auto const& obj) { return obj.get_description(); }, alg);
  ui.description->setText(QString(desc.c_str()));

  QGridLayout* layout = new QGridLayout;
  int nparams = alg->get_params().size();
  // std::visit([](auto const& obj) { return obj.get_params().size(); }, alg);
  for (int i = 0; i < nparams; ++i)
  {
    // get the i-th param from the variant algorithm list
    auto p = alg->get_params()[i];

    // draw datasets in left column, params_ in right
    int column = std::visit([&](auto const& v) { return get_column(v); }, p.value);

    // get label for parameter
    QLabel* const label = new QLabel(p.name);
    layout->addWidget(label, counts[column], column * 2);

    // get a widget to represent the parameter (based on param type)
    QWidget* widget = std::visit([&](auto const& v) { return get_widget(v); }, p.value);
    layout->addWidget(widget, counts[column], column * 2 + 1);
    params_ << widget;
    //
    counts[column]++;
  }
  ui.algo_params->setLayout(layout);
  // compute the new best guess size
  adjustSize();
  // force a resize to fit best guess
  resize(sizeHint());
}

// ----------------------------------------------------------------------------
// copy user params_ from dialog into default indicator param object
// so that they persist and are there again next time the dialog is opened
void indicator_widget::update_parameters()
{
  int index = ui.algorithm->currentIndex();
  // get a reference to indicator in the global indicators list
  auto& alg = indicators_[index];

  // create a new param list from the gui widget
  indicators::param_list new_params;
  new_params = alg->get_params();
  // std::visit([](auto const& obj) { return obj.get_params(); }, alg);

  for (int i = 0; i < new_params.size(); ++i)
  {
    // get the widget that represents the param
    QWidget* widget = params_[i];

    // update the param value from the widget
    std::visit([i, widget](auto& value) { set_param(widget, value); }, new_params[i].value);
  }

  // overwrite the original params_ with the new default / updated values
  alg->set_params(new_params);
  // std::visit([&](auto& obj) { obj.set_params(new_params); }, alg);
}
