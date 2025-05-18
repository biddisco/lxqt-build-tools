#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
//
#include "widgets_control/control_builder.hpp"

// ----------------------------------------------------------------------------
QWidget* int_control_builder::build(QWidget* parent, ControlConfig const& config)
{
  auto* spin = new QSpinBox(parent);
  if (config.contains("min")) spin->setMinimum(config.at("min").toInt());
  if (config.contains("max")) spin->setMaximum(config.at("max").toInt());
  return spin;
}

// ----------------------------------------------------------------------------
QWidget* string_control_builder::build(QWidget* parent, ControlConfig const& config)
{
  auto* edit = new QLineEdit(parent);
  if (config.contains("placeholder")) edit->setPlaceholderText(config.at("placeholder").toString());
  return edit;
}

// ----------------------------------------------------------------------------
QWidget* combo_control_builder::build(QWidget* parent, ControlConfig const& config)
{
  auto* edit = new QComboBox(parent);
  if (config.contains("entries"))
  {
    edit->addItems(config.at("entries").toStringList());
    if (config.contains("index")) { edit->setCurrentIndex(config.at("index").toInt()); }
    else { edit->setCurrentText(config.at("entries").toStringList()[0]); }
  }
  else if (config.contains("placeholder"))
  {
    edit->setPlaceholderText(config.at("placeholder").toString());
  }
  return edit;
}
