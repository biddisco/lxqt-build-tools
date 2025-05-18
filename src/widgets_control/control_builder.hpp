#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <QString>
#include <QWidget>

// ----------------------------------------------------------------------------
using ControlConfig = std::map<QString, QVariant>;
using NestedControlConfigs = std::map<QString, ControlConfig>;

// ----------------------------------------------------------------------------
class control_builder
{
  public:
  virtual QWidget* build(QWidget* parent, ControlConfig const& config) = 0;
  virtual ~control_builder() = default;
};

// ----------------------------------------------------------------------------
class int_control_builder : public control_builder
{
  public:
  QWidget* build(QWidget* parent, ControlConfig const& config) override;
};

// ----------------------------------------------------------------------------
class string_control_builder : public control_builder
{
  public:
  QWidget* build(QWidget* parent, ControlConfig const& config) override;
};

// ----------------------------------------------------------------------------
class combo_control_builder : public control_builder
{
  public:
  QWidget* build(QWidget* parent, ControlConfig const& config) override;
};
