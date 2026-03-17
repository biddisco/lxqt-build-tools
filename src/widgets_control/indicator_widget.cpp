// STL
#include <array>
#include <cstddef>
#include <string>
#include <vector>
// Qt
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
// Grox
#include "ui_indicator_widget.h"
//
#include "debug/logging.hpp"
#include "indicators/indicator_registry.hpp"
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_factory.hpp"
#include "widgets_control/indicator_json.hpp"
#include "widgets_control/indicator_widget.hpp"

// ----------------------------------------------------------------------------
// Create a widget with a combo selection for each indicator to choose from
// when selected, build a control with the paramaters for the indicator
indicator_widget::indicator_widget(indicators::indicator_vector const* vec, std::size_t& index)
  : QDialog()
  , indicators_(vec)
  , algorithm_{}
  , indicator_widget_(nullptr)
{
  if (index >= indicators_->size())
  {
    GROX_LOG_ERROR(indicator_log, "{:>20} index out of range index={} size={}", "indicator_widget",
        index, indicators_->size());
    return;
  }
  ui = new Ui::indicator_widget();
  ui->setupUi(this);
  this->setWindowTitle("Indicator");

  // setup algorithms combobox
  for (auto const& a : *indicators_)
  {
    QString s = a->get_name().c_str();
    ui->algorithm->addItem(s);
  }
  // when algorithm is changed, rebuild gui
  connect(
      ui->algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      [&, this](int i) {
        index = i;    // make sure the global index is updated for next time the dialog is opened
        refresh_gui(indicators_->operator[](i));
      },
      Qt::QueuedConnection);

  // build gui for first/last used algorithm
  ui->algorithm->setCurrentIndex(index);
  refresh_gui(indicators_->operator[](index));
}

// ----------------------------------------------------------------------------
// Create a widget dialog dedicated to only a single algorithm
indicator_widget::indicator_widget(indicators::shared_algorithm alg, nlohmann::json values)
  : QDialog()
  , indicators_{nullptr}
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
  return (indicators_->operator[](index));
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
  // add ok, cancel buttons - Pass dlg as parent immediately
  QDialogButtonBox* buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, dlg);

  connect(buttonBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
  connect(buttonBox, &QDialogButtonBox::clicked, this, [=, this](QAbstractButton* b) {
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
  // Keep the UI layout created by setupUi() so all controls resize together.
  while (auto* item = ui->buttons_layout->takeAt(0))
  {
    if (item->widget()) item->widget()->deleteLater();
    delete item;
  }
  ui->buttons_layout->addStretch(1);
  ui->buttons_layout->addWidget(buttonBox);
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

  // add a duration combo — this is an execution property, not an indicator parameter
  auto* duration_layout = new QFormLayout();
  duration_combo_ = new QComboBox(ui->algo_params);
  duration_combo_->setObjectName("duration");
  for (auto const& s : sample_duration::durations) { duration_combo_->addItem(s); }
  // find the resolution from the candle_data param and convert current duration to string
  candle_res res = ohlc_data_resolutions::minute15;
  for (auto const& p : alg->get_params())
  {
    if (auto const* d = std::get_if<indicators::param<candle_data>>(&p))
    {
      res = d->get().res_;
      break;
    }
  }
  duration_combo_->setCurrentText(
      QString::fromStdString(sample_duration::to_string(res, alg->get_duration())));
  duration_layout->addRow("Duration:", duration_combo_);
  layout->addLayout(duration_layout);

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
    bool is_indicator_ref = false;
    std::visit(
        [&](auto const& v) {
          pname = v.name_;
          ptype = grox::debug::print_type<typeof(v.val_)>();
          if constexpr (std::is_same_v<std::decay_t<decltype(v.val_)>, indicator_ref>)
            is_indicator_ref = true;
        },
        new_params[i]);

    if (is_indicator_ref)
    {
      // Read sub-indicator params from nested form widget
      QWidget* nested = static_cast<QWidget*>(widget_map[pname].value<void*>());
      if (nested)
      {
        QMap<QString, QVariant> sub_map =
            nested->property("ParamWidgets").value<QMap<QString, QVariant>>();
        auto& ref = std::get<indicators::param<indicator_ref>>(new_params[i]);
        if (ref.get_ref().prototype_)
        {
          auto sub_params = ref.get_ref().prototype_->get_params();
          for (int j = 0; j < sub_params.size(); ++j)
          {
            std::string sub_ptype;
            QString sub_pname;
            bool is_candle = false;
            std::visit(
                [&](auto const& sv) {
                  sub_pname = sv.name_;
                  sub_ptype = grox::debug::print_type<typeof(sv.val_)>();
                  if constexpr (std::is_same_v<std::decay_t<decltype(sv.val_)>, candle_data>)
                    is_candle = true;
                },
                sub_params[j]);
            // Skip candle_data — sub-indicators inherit from parent
            if (is_candle) continue;
            if (sub_map.contains(sub_pname))
            {
              QWidget* sw = static_cast<QWidget*>(sub_map[sub_pname].value<void*>());
              sub_params[j] = factory.get_value_from_control(to_qstring(sub_ptype), sw);
            }
          }
          ref.get_ref().prototype_->set_params(sub_params);
        }
      }
    }
    else
    {
      // get the widget that represents the param
      QWidget* param_widget = static_cast<QWidget*>(widget_map[pname].value<void*>());
      new_params[i] = factory.get_value_from_control(to_qstring(ptype), param_widget);
    }
  }

  // update the param value from the widget
  // std::visit([i, widget](auto& p) { set_param(widget, p); }, new_params[i]);

  // overwrite the original params with the new default / updated values
  alg->set_params(new_params);

  // update duration from the duration combo (execution property, not a param)
  if (duration_combo_)
  {
    // find the resolution from the candle_data param
    candle_res res = ohlc_data_resolutions::minute15;
    for (auto const& p : new_params)
    {
      if (auto const* d = std::get_if<indicators::param<candle_data>>(&p))
      {
        res = d->get().res_;
        break;
      }
    }
    std::string dur_str = duration_combo_->currentText().toStdString();
    alg->set_duration(sample_duration::to_samples(res, dur_str));
  }
  // std::visit([&](auto& obj) { obj.set_params(new_params); }, alg);
}
