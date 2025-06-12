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
#include "indicators/indicator_types.hpp"
#include "util/stringutils.hpp"
#include "widgets_control/control_builder.hpp"

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

  void register_builder(QString const& typeName, std::unique_ptr<control_builder> builder)
  {
    if (!builders.contains(typeName)) builders.insert(std::make_pair(typeName, std::move(builder)));
  }

  QWidget* create_control(
      QString const& typeName, QWidget* parent, QString name, nlohmann::json const& config = {})
  {
    if (builders.contains(typeName)) { return builders[typeName]->build(parent, name, config); }
    return new QLabel("Unsupported type: " + typeName, parent);
  }

  indicators::variant_type get_value_from_control(QString const& typeName, QWidget* widget)
  {
    if (builders.contains(typeName)) { return builders[typeName]->get_value(widget); }
    return {};
  }

  private:
  std::map<QString, std::unique_ptr<control_builder>> builders;

  // prevent copy
  control_factory(control_factory const&) = delete;
  control_factory& operator=(control_factory const&) = delete;
};

// ----------------------------------------------------------------------------
static QWidget* build_control(nlohmann::ordered_json const& json, nlohmann::json const& defaults,
    control_factory& factory, QWidget* parent = nullptr)
{
  QWidget* container = new QWidget(parent);
  auto* layout = new QFormLayout(container);

  // for properties, we must use types the Qt metatype system understands, so QVariant
  QMap<QString, QVariant> param_widgets;
  for (nlohmann::ordered_json::const_iterator it = json.begin(); it != json.end(); ++it)
  {
    std::string name = it.key();
    QString qname = to_qstring(name);

    if (it->is_structured())
    {
      auto config = defaults.contains(name) ? defaults[name] : nlohmann::json{};
      QWidget* nestedForm = build_control(*it, config, factory, container);
      QGroupBox* group = new QGroupBox(qname, container);
      auto* groupLayout = new QVBoxLayout(group);
      groupLayout->addWidget(nestedForm);
      layout->addRow(group);
    }
    else
    {
      QString type = to_qstring(it.value());
      // fetch the user config for this field
      auto config = defaults.contains(name) ? defaults[name] : nlohmann::json{};
      QWidget* control = factory.create_control(type, container, qname, config);
      param_widgets[qname] = QVariant::fromValue<void*>(control);
      if (config.contains("label"))
        layout->addRow(to_qstring(config["label"].get<std::string>() + ":"), control);
      else
        layout->addRow(qname + ":", control);
    }
  }
  container->setProperty("ParamWidgets", QVariant(param_widgets));
  return container;
}
