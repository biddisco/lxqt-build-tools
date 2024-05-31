// STL
#include <string>
#include <vector>
// Qt
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QString>
// Grox
#include "data/ohlc_datasets.hpp"
#include "indicators/indicator_definitions.hpp"
#include "widgets/indicator_dialog.hpp"

// ----------------------------------------------------------------------------
indicator_dialog::indicator_dialog()
  : QDialog()
{
  static int last_selected_index = 0;

  ui.setupUi(this);
  this->setWindowTitle("Indicator");

  // add ok, cancel buttons
  QDialogButtonBox* buttonBox =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
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
  for (auto const& a : indicators::available_indicators)
  {
    QString s = std::visit([](auto const& obj) { return obj.name; }, a).c_str();
    ui.algorithm->addItem(s);
  }
  // when algorithm is changed, rebuild gui
  connect(
    ui.algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
    [this](int index) {
      last_selected_index = index;
      refresh_gui(index);
    },
    Qt::QueuedConnection);

  // build gui for first/last used algorithm
  ui.algorithm->setCurrentIndex(last_selected_index);
  refresh_gui(last_selected_index);
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
// we must provide one overload for each type in indicators::param_types
QWidget* get_widget(double const& param)
{
  QLineEdit* const widget = new QLineEdit();
  widget->setValidator(new QDoubleValidator(-10.0E9, 10.0E9, 5, widget));
  widget->setText(QString::number(param));
  return widget;
}

QWidget* get_widget(int const& param)
{
  QLineEdit* const widget = new QLineEdit();
  widget->setValidator(new QIntValidator(0, 65535, widget));
  widget->setText(QString::number(param));
  return widget;
}

QWidget* get_widget(ohlc_modes const& param)
{
  QStringList mode_list;
  for (auto const& r : ohlc_mode_names)
  {
    mode_list << QString::fromStdString(std::string(r));
  }

  QComboBox* const widget = new QComboBox();
  widget->addItems(mode_list);
  widget->setCurrentText(QString::fromStdString(std::string(magic_enum::enum_name(param))));
  return widget;
}

QWidget* get_widget(bool const& param)
{
  QCheckBox* const widget = new QCheckBox();
  widget->setChecked(param);
  return widget;
}

QWidget* get_widget(candle_res const& param)
{
  QStringList res_list;
  for (auto const& r : ohlc_data_resolutions::available_resolutions())
  {
    res_list << r.name_;
  }
  //
  QComboBox* const widget = new QComboBox();
  widget->addItems(res_list);
  widget->setCurrentText(param.name_);
  return widget;
}

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

void set_param(QWidget* widget, double& param)
{
  QLineEdit* w = dynamic_cast<QLineEdit*>(widget);
  param = QLocale().toDouble(w->text(), nullptr);
}

void set_param(QWidget* widget, int& param)
{
  QLineEdit* w = dynamic_cast<QLineEdit*>(widget);
  param = QLocale().toInt(w->text(), nullptr);
}

void set_param(QWidget* widget, bool& param)
{
  QCheckBox* w = dynamic_cast<QCheckBox*>(widget);
  param = w->isChecked();
}

void set_param(QWidget* widget, candle_res& param)
{
  QComboBox* w = dynamic_cast<QComboBox*>(widget);
  int index = w->currentIndex();
  param = ohlc_data_resolutions::available_resolutions()[index];
}

void set_param(QWidget* widget, ohlc_modes& param)
{
  QComboBox* w = dynamic_cast<QComboBox*>(widget);
  int index = w->currentIndex();
  param = magic_enum::enum_value<ohlc_modes>(index);
  ;
}

// ----------------------------------------------------------------------------
void indicator_dialog::refresh_gui(int index)
{
  // wipe the contents of the dialog
  QLayout* oldlayout = ui.algo_params->layout();
  if (oldlayout)
    clearLayout(oldlayout, true);
  params.clear();
  std::array<int, 2> counts = {0, 0};

  auto alg = indicators::available_indicators[index];
  std::string desc = std::visit([](auto const& obj) { return obj.description; }, alg);
  ui.description->setText(QString(desc.c_str()));

  QGridLayout* layout = new QGridLayout;
  int nparams = std::visit([](auto const& obj) { return obj.params.size(); }, alg);
  for (int i = 0; i < nparams; ++i)
  {
    // get the i-th param from the variant algorithm list
    auto p = std::visit([=](auto const& obj) { return obj.params[i]; }, alg);
    // draw datasets in left column, params in right
    int column = std::visit([&](auto const& v) { return get_column(v); }, std::get<1>(p));

    // get label for parameter
    QLabel* const label = new QLabel(std::get<0>(p));
    layout->addWidget(label, counts[column], column * 2);

    // get a widget to represent the parameter (based on param type)
    QWidget* widget = std::visit([&](auto const& v) { return get_widget(v); }, std::get<1>(p));
    layout->addWidget(widget, counts[column], column * 2 + 1);
    params << widget;
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
// copy user params from dialog into default indicator param object
// so that they are there again next time the dialog is opened
void indicator_dialog::update_parameters()
{
  int index = ui.algorithm->currentIndex();
  // get a reference to indicator in the global indicators list
  auto& alg = indicators::available_indicators[index];
  // get the number of params it has
  int nparams = std::visit([](auto const& obj) { return obj.params.size(); }, alg);
  for (int i = 0; i < nparams; ++i)
  {
    // get a reference to the i-th param from the variant algorithm list
    auto& p = std::visit(
      [=](auto& obj) -> auto& { return obj.params[i]; }, alg);

    // get the widget that represents the param
    QWidget* widget = params[i];
    // update the param value from the widget
    std::visit([&](auto& v) { set_param(widget, v); }, std::get<1>(p));
  }
}

// ----------------------------------------------------------------------------
void indicator_dialog::done(int r)
{
  if (QDialog::Accepted == r)    // ok was pressed
  {
    update_parameters();
  }
  QDialog::done(r);
}
