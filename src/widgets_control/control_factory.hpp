#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
//
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_field_data.hpp"

// ----------------------------------------------------------------------------
class ControlFactory
{
  public:
  ControlFactory() {}

  void registerBuilder(QString const& typeName, std::unique_ptr<control_builder> builder)
  {
    builders.insert(std::make_pair(typeName, std::move(builder)));
  }

  QWidget* createControl(QString const& typeName, QWidget* parent, ControlConfig const& config = {})
  {
    if (builders.contains(typeName)) { return builders[typeName]->build(parent, config); }
    return new QLabel("Unsupported type: " + typeName, parent);
  }

  private:
  std::map<QString, std::unique_ptr<control_builder>> builders;

  // prevent copy
  ControlFactory(ControlFactory const&) = delete;
  ControlFactory& operator=(ControlFactory const&) = delete;
};

// ----------------------------------------------------------------------------
QWidget* build_control(std::list<control_field_data> const& fields,
    NestedControlConfigs const& defaults, ControlFactory& factory, QWidget* parent = nullptr,
    QString const& prefix = "")
{
  QWidget* container = new QWidget(parent);
  auto* layout = new QFormLayout(container);

  for (control_field_data const& field : fields)
  {
    QString fullName = prefix.isEmpty() ? field.name : prefix + "." + field.name;

    if (field.type == "object")
    {
      QWidget* nestedForm = build_control(field.subfields, defaults, factory, container, fullName);
      QGroupBox* group = new QGroupBox(field.name, container);
      auto* groupLayout = new QVBoxLayout(group);
      groupLayout->addWidget(nestedForm);
      layout->addRow(group);
    }
    else
    {
      ControlConfig config = defaults.at(fullName);
      QWidget* control = factory.createControl(field.type, container, config);
      layout->addRow(field.name + ":", control);
    }
  }

  return container;
}
