#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <QString>
#include <QWidget>

// ----------------------------------------------------------------------------
using control_config = std::map<QString, QVariant>;
using nested_control_configs = std::map<QString, control_config>;

// ----------------------------------------------------------------------------
class control_builder
{
  public:
  virtual QWidget* build(QWidget* parent, control_config const& config) = 0;
  virtual ~control_builder() = default;
};

// ----------------------------------------------------------------------------
class int_control_builder : public control_builder
{
  public:
  QWidget* build(QWidget* parent, control_config const& config) override;
};

// ----------------------------------------------------------------------------
class string_control_builder : public control_builder
{
  public:
  QWidget* build(QWidget* parent, control_config const& config) override;
};

// ----------------------------------------------------------------------------
class combo_control_builder : public control_builder
{
  public:
  QWidget* build(QWidget* parent, control_config const& config) override;
};
