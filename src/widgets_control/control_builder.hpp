#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <QString>
#include <QWidget>
//
#include <nlohmann/json.hpp>
//
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
void register_control_factories();

// ----------------------------------------------------------------------------
class control_builder
{
  public:
  virtual QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) = 0;
  virtual indicators::variant_type get_value(QWidget* widget) = 0;
  virtual ~control_builder() = default;
};

// ----------------------------------------------------------------------------
class control_builder_int : public control_builder
{
  public:
  QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) override;
  indicators::variant_type get_value(QWidget* widget) override;
};

// ----------------------------------------------------------------------------
class control_builder_bool : public control_builder
{
  public:
  QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) override;
  indicators::variant_type get_value(QWidget* widget) override;
};

// ----------------------------------------------------------------------------
class control_builder_double : public control_builder
{
  public:
  QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) override;
  indicators::variant_type get_value(QWidget* widget) override;
};

// ----------------------------------------------------------------------------
class control_builder_string : public control_builder
{
  public:
  QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) override;
  indicators::variant_type get_value(QWidget* widget) override;
};

// ----------------------------------------------------------------------------
class control_builder_combo : public control_builder
{
  public:
  QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) override;
  indicators::variant_type get_value(QWidget* widget) override;
};

// ----------------------------------------------------------------------------
class control_builder_ohlc_mode : public control_builder
{
  public:
  QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) override;
  indicators::variant_type get_value(QWidget* widget) override;
};

// ----------------------------------------------------------------------------
class control_builder_candle_data : public control_builder
{
  public:
  QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) override;
  indicators::variant_type get_value(QWidget* widget) override;
};

// ----------------------------------------------------------------------------
class control_builder_orderbook : public control_builder
{
  public:
  QWidget* build(QWidget* parent, QString name, nlohmann::json const& defaults) override;
  indicators::variant_type get_value(QWidget* widget) override;
};
