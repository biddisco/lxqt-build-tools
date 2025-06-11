#pragma once

#include <iostream>
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
#include <nlohmann/json.hpp>
//
#include "util/stringutils.hpp"
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_field_data.hpp"

// ----------------------------------------------------------------------------
class control_factory
{
  public:
  control_factory() {}

  static control_factory& getInstance()
  {
    static control_factory instance;
    return instance;
  }

  void registerBuilder(QString const& typeName, std::unique_ptr<control_builder> builder)
  {
    builders.insert(std::make_pair(typeName, std::move(builder)));
  }

  QWidget* createControl(
      QString const& typeName, QWidget* parent, nlohmann::json const& config = {})
  {
    if (builders.contains(typeName)) { return builders[typeName]->build(parent, config); }
    return new QLabel("Unsupported type: " + typeName, parent);
  }

  private:
  std::map<QString, std::unique_ptr<control_builder>> builders;

  // prevent copy
  control_factory(control_factory const&) = delete;
  control_factory& operator=(control_factory const&) = delete;
};

// ----------------------------------------------------------------------------
static QWidget* build_control(nlohmann::ordered_json const& json, nlohmann::json const& defaults,
    control_factory& factory, QWidget* parent = nullptr, QString const& prefix = "")
{
  QWidget* container = new QWidget(parent);
  auto* layout = new QFormLayout(container);

  for (nlohmann::ordered_json::const_iterator it = json.begin(); it != json.end(); ++it)
  {
    QString name = to_qstring(it.key());
    QString fullName = prefix.isEmpty() ? name : prefix + "." + name;

    if (it->is_structured())
    {
      auto config = defaults.contains(it.key()) ? defaults[it.key()] : nlohmann::json{};
      QWidget* nestedForm = build_control(*it, config, factory, container, fullName);
      QGroupBox* group = new QGroupBox(name, container);
      auto* groupLayout = new QVBoxLayout(group);
      groupLayout->addWidget(nestedForm);
      layout->addRow(group);
    }
    else
    {
      QString type = to_qstring(it.value());
      // fetch the user config for this field
      auto config = defaults.contains(fullName) ? defaults[fullName] : nlohmann::json{};
      QWidget* control = factory.createControl(type, container, config);
      if (config.contains("label"))
        layout->addRow(to_qstring(config["label"].get<std::string>() + ":"), control);
      else
        layout->addRow(name + ":", control);
    }
  }

  return container;
}
