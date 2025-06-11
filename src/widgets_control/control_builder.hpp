#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <nlohmann/json.hpp>
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
  virtual QWidget* build(QWidget* parent, nlohmann::json const& defaults) = 0;
  virtual ~control_builder() = default;
};

// ----------------------------------------------------------------------------
class control_builder_int : public control_builder
{
  public:
  QWidget* build(QWidget* parent, nlohmann::json const& defaults) override;
};

// ----------------------------------------------------------------------------
class control_builder_bool : public control_builder
{
  public:
  QWidget* build(QWidget* parent, nlohmann::json const& defaults) override;
};

// ----------------------------------------------------------------------------
class control_builder_double : public control_builder
{
  public:
  QWidget* build(QWidget* parent, nlohmann::json const& defaults) override;
};

// ----------------------------------------------------------------------------
class control_builder_string : public control_builder
{
  public:
  QWidget* build(QWidget* parent, nlohmann::json const& defaults) override;
};

// ----------------------------------------------------------------------------
class control_builder_combo : public control_builder
{
  public:
  QWidget* build(QWidget* parent, nlohmann::json const& defaults) override;
};

// ----------------------------------------------------------------------------
class control_builder_ohlc_mode : public control_builder
{
  public:
  QWidget* build(QWidget* parent, nlohmann::json const& defaults) override;
};

// ----------------------------------------------------------------------------
class control_builder_candle_data : public control_builder
{
  public:
  QWidget* build(QWidget* parent, nlohmann::json const& defaults) override;
};

// ----------------------------------------------------------------------------
class control_builder_orderbook : public control_builder
{
  public:
  QWidget* build(QWidget* parent, nlohmann::json const& defaults) override;
};
