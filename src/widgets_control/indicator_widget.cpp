// STL
#include <array>
#include <cstddef>
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
#include "ui_indicator_widget.h"
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_factory.hpp"
#include "widgets_control/indicator_json.hpp"
#include "widgets_control/indicator_widget.hpp"

// ----------------------------------------------------------------------------
// Create a widget with a combo selection for each indicator to choose from
// when selected, build a control with the paramaters for the indicator
indicator_widget::indicator_widget(indicators::indicator_vector const& vec, std::size_t& index)
  : QDialog()
  , indicators_(vec)
  , algorithm_{}
  , indicator_widget_(nullptr)
{
  ui = new Ui::indicator_widget();
  ui->setupUi(this);
  this->setWindowTitle("Indicator");

  // setup algorithms combobox
  for (auto const& a : indicators_)
  {
    QString s = a->get_name().c_str();
    ui->algorithm->addItem(s);
  }
  // when algorithm is changed, rebuild gui
  connect(
      ui->algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      [&, this](int i) {
        index = i;    // make sure the global index is updated for next time the dialog is opened
        refresh_gui(indicators_[i]);
      },
      Qt::QueuedConnection);

  // build gui for first/last used algorithm
  ui->algorithm->setCurrentIndex(index);
  refresh_gui(indicators_[index]);
}

// ----------------------------------------------------------------------------
// Create a widget dialog dedicated to only a soiongle algorithm
indicator_widget::indicator_widget(indicators::shared_algorithm alg, nlohmann::json values)
  : QDialog()
  , indicators_{}
  , algorithm_(alg)
  , indicator_widget_(nullptr)
{
  ui = new Ui::indicator_widget();
  ui->setupUi(this);
  this->setWindowTitle("Indicator");

  // hide algorithms combobox
  ui->algorithm->hide();

  // build gui for first/last used algorithm
  refresh_gui(alg, values);
}

// ----------------------------------------------------------------------------
indicator_widget::~indicator_widget()
{
  delete indicator_widget_;
  delete ui;
}

// ----------------------------------------------------------------------------
// return a variant containing a copy of the selected algorithm
// including all parameters set by the user in the dialog
indicators::shared_algorithm indicator_widget::get_algorithm()
{
  int index = ui->algorithm->currentIndex();
  if (index == -1) return algorithm_;
  return indicators_[index];
}

// ----------------------------------------------------------------------------
int indicator_widget::execute_as_dialog()
{
  add_indicator_to_dialog(this);
  return exec();
}

// ----------------------------------------------------------------------------
// Insert the widget created by this class into an existing dialog
// add ok, cancel reset buttons to the dialog along with the controls
void indicator_widget::add_indicator_to_dialog(QDialog* dlg)
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
  ui->buttons_layout->addWidget(buttonBox);
  //
  QVBoxLayout* VLayout = new QVBoxLayout(dlg);
  VLayout->addWidget(this);
  dlg->setLayout(VLayout);
}

// ----------------------------------------------------------------------------
// from the indicator algorithm, generate the gui controls
QWidget* create_indicator_control(indicators::shared_algorithm alg, nlohmann::json values = {})
{
  // @TODO: only need to do this once on startup
  register_control_factories();
  //
  nlohmann::ordered_json controls = get_json_layout_indicator(alg);
  if (values.size() == 0) values = get_json_values_indicator(alg);
  //
  auto widget = build_control(controls, values, control_factory::getInstance());
  widget->setWindowTitle(to_qstring(alg->get_name()));
  return widget;
}

// ----------------------------------------------------------------------------
// populate the widget with controls for the indicator algorithm parameters
void indicator_widget::refresh_gui(indicators::shared_algorithm alg, nlohmann::json values)
{
  // set the description field
  std::string desc = alg->get_description();
  ui->description->setText(QString(desc.c_str()));

  // wipe contents: transfer layout to temp widget and children will be deleted on destruction
  if (ui->algo_params->layout()) QWidget().setLayout(ui->algo_params->layout());

  // create a new main layout for algo_params widgets
  QVBoxLayout* layout = new QVBoxLayout(ui->algo_params);
  // get the currently selected algorithm
  indicator_widget_ = create_indicator_control(alg, values);
  layout->addWidget(indicator_widget_);
  // compute the new best guess size
  adjustSize();
  // "layout takes responsibility to automatically resize when widgets are shown or hidden"
  // SetFixedSize: The main widget's size is set to sizeHint(); it cannot be resized at all.
  if (parentWidget()) { parentWidget()->layout()->setSizeConstraint(QLayout::SetFixedSize); }
}

// ----------------------------------------------------------------------------
// copy user params from dialog into default indicator param object
// so that they persist and are there again next time the dialog is opened
void indicator_widget::update_parameters()
{
  auto alg = get_algorithm();

  // create a new param list from the gui widget
  indicators::param_list new_params = alg->get_params();
  QMap<QString, QVariant> widget_map =
      indicator_widget_->property("ParamWidgets").value<QMap<QString, QVariant>>();

  control_factory& factory = control_factory::getInstance();
  for (int i = 0; i < new_params.size(); ++i)
  {
    // get the name of the param
    std::string ptype;
    QString pname;
    std::visit(
        [&](auto const& v) {
          pname = v.name_;
          ptype = grox::debug::print_type<typeof(v.val_)>();
        },
        new_params[i]);
    // get the widget that represents the param
    QWidget* param_widget = static_cast<QWidget*>(widget_map[pname].value<void*>());
    new_params[i] = factory.get_value_from_control(to_qstring(ptype), param_widget);
  }

  // update the param value from the widget
  // std::visit([i, widget](auto& p) { set_param(widget, p); }, new_params[i]);

  // overwrite the original params with the new default / updated values
  alg->set_params(new_params);
  // std::visit([&](auto& obj) { obj.set_params(new_params); }, alg);
}
