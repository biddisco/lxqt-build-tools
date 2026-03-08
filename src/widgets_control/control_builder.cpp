#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
//
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
//
#include "config/config.hpp"
#include "exchange/abstract_exchange.hpp"
#include "indicators/indicator_types.hpp"
#include "util/stringutils.hpp"
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_factory.hpp"

// ----------------------------------------------------------------------------
// function that registers all the default control builders
// ----------------------------------------------------------------------------
void register_control_factories()
{
  control_factory& factory = control_factory::getInstance();
  factory.register_builder("int", std::make_unique<control_builder_int>());
  factory.register_builder("bool", std::make_unique<control_builder_bool>());
  factory.register_builder("double", std::make_unique<control_builder_double>());
  factory.register_builder("string", std::make_unique<control_builder_string>());
  factory.register_builder("combo", std::make_unique<control_builder_combo>());
  factory.register_builder("ohlc_modes", std::make_unique<control_builder_ohlc_mode>());
  factory.register_builder("candle_data", std::make_unique<control_builder_candle_data>());
  factory.register_builder("order_book_param", std::make_unique<control_builder_orderbook>());
}

// ----------------------------------------------------------------------------
// int
// ----------------------------------------------------------------------------
QWidget* control_builder_int::build(QWidget* parent, QString name, nlohmann::json const& defaults)
{
  auto* edit = new QLineEdit(parent);
  edit->setObjectName(name);
  int value = 0;
  int min = 0;
  int max = 255;
  // auto* spin = new QSpinBox(parent);
  if (defaults.contains("min")) min = defaults["min"].get<double>();
  if (defaults.contains("max")) max = defaults["max"].get<double>();
  if (defaults.contains("value")) value = defaults["value"].get<double>();
  edit->setValidator(new QIntValidator(min, max, edit));
  edit->setText(QString::number(value));
  return edit;
}

// ----------------------------------------------------------------------------
indicators::variant_type control_builder_int::get_value(QWidget* widget)
{
  bool ok;
  QLineEdit* edit = static_cast<QLineEdit*>(widget);
  int num = edit->text().toInt(&ok);
  if (!ok) { throw std::runtime_error("Int conversion failed"); }
  return indicators::param<int>(edit->objectName(), num);
}

// ----------------------------------------------------------------------------
// bool
// ----------------------------------------------------------------------------
QWidget* control_builder_bool::build(QWidget* parent, QString name, nlohmann::json const& defaults)
{
  auto* box = new QCheckBox(parent);
  box->setObjectName(name);
  if (defaults.contains("value")) box->setChecked(defaults["value"].get<bool>());
  return box;
}

// ----------------------------------------------------------------------------
indicators::variant_type control_builder_bool::get_value(QWidget* widget)
{
  QCheckBox* box = static_cast<QCheckBox*>(widget);
  bool checked = box->isChecked();
  return indicators::param<bool>(box->objectName(), checked);
}

// ----------------------------------------------------------------------------
// double
// ----------------------------------------------------------------------------
QWidget* control_builder_double::build(
    QWidget* parent, QString name, nlohmann::json const& defaults)
{
  auto* edit = new QLineEdit(parent);
  edit->setObjectName(name);
  double value = 0;
  double min = -10.0E9;
  double max = 10.0E9;
  int decimals = 6;
  if (defaults.contains("min")) min = defaults["min"].get<double>();
  if (defaults.contains("max")) max = defaults["max"].get<double>();
  if (defaults.contains("decimals")) decimals = defaults["decimals"].get<int>();
  if (defaults.contains("value")) value = defaults["value"].get<double>();
  edit->setValidator(new QDoubleValidator(min, max, decimals, edit));
  edit->setText(QString::number(value));
  return edit;
}

// ----------------------------------------------------------------------------
indicators::variant_type control_builder_double::get_value(QWidget* widget)
{
  bool ok;
  QLineEdit* edit = static_cast<QLineEdit*>(widget);
  double num = edit->text().toDouble(&ok);
  if (!ok) { throw std::runtime_error("Double conversion failed"); }
  return indicators::param<double>(edit->objectName(), num);
}

// ----------------------------------------------------------------------------
// string
// ----------------------------------------------------------------------------
QWidget* control_builder_string::build(
    QWidget* parent, QString name, nlohmann::json const& defaults)
{
  auto* edit = new QLineEdit(parent);
  edit->setObjectName(name);
  if (defaults.contains("placeholder"))
    edit->setPlaceholderText(to_qstring(defaults["placeholder"].get<std::string>()));
  if (defaults.contains("value")) edit->setText(to_qstring(defaults["value"].get<std::string>()));
  return edit;
}

// ----------------------------------------------------------------------------
indicators::variant_type control_builder_string::get_value(QWidget* widget)
{
  QLineEdit* edit = static_cast<QLineEdit*>(widget);
  return indicators::param<std::string>(edit->objectName(), edit->text().toStdString());
}

// ----------------------------------------------------------------------------
// combo
// ----------------------------------------------------------------------------
QWidget* control_builder_combo::build(QWidget* parent, QString name, nlohmann::json const& defaults)
{
  auto* combo = new QComboBox(parent);
  combo->setObjectName(name);
  if (defaults.contains("entries"))
  {
    combo->addItems(to_qstringlist(defaults["entries"].get<std::vector<std::string>>()));
    if (defaults.contains("index")) { combo->setCurrentIndex(defaults["index"].get<int>()); }
    else
    {
      combo->setCurrentText(to_qstringlist(defaults["entries"].get<std::vector<std::string>>())[0]);
    }
  }
  else if (defaults.contains("placeholder"))
  {
    combo->setPlaceholderText(to_qstring(defaults["placeholder"].get<std::string>()));
  }
  return combo;
}

// ----------------------------------------------------------------------------
indicators::variant_type control_builder_combo::get_value(QWidget* widget)
{
  QComboBox* combo = static_cast<QComboBox*>(widget);
  return indicators::param<int>(combo->objectName(), combo->currentIndex());
}

// ----------------------------------------------------------------------------
// ohlc_mode
// ----------------------------------------------------------------------------
QWidget* control_builder_ohlc_mode::build(
    QWidget* parent, QString name, nlohmann::json const& defaults)
{
  // just use a combo box for now
  auto builder = control_builder_combo();
  return builder.build(parent, name, defaults);
}

// ----------------------------------------------------------------------------
indicators::variant_type control_builder_ohlc_mode::get_value(QWidget* widget)
{
  auto builder = control_builder_combo();
  int index = std::get<indicators::param<int>>(builder.get_value(widget)).get();
  return indicators::param<ohlc_modes>(
      widget->objectName(), magic_enum::enum_value<ohlc_modes>(index));
}

// ----------------------------------------------------------------------------
// candle_data — only shows resolution (duration is handled by indicator_widget)
// ----------------------------------------------------------------------------
QWidget* control_builder_candle_data::build(
    QWidget* parent, QString name, nlohmann::json const& defaults)
{
  QFrame* widget = new QFrame(parent);
  widget->setObjectName(name);
  QVBoxLayout* layout = new QVBoxLayout(widget);
  widget->setLayout(layout);

  // combo box of resolutions to choose from
  QComboBox* const combo = new QComboBox(widget);
  QStringList resolutions;
  if (defaults.contains("resolutions"))
    resolutions = to_qstringlist(defaults["resolutions"].get<std::vector<std::string>>());
  else
    for (auto const& r : ohlc_data_resolutions::available_resolutions()) { resolutions << r.name_; }
  combo->setObjectName("resolution");
  combo->addItems(resolutions);
  if (defaults.contains("resolution"))
    combo->setCurrentText(to_qstring(defaults["resolution"].get<std::string>()));
  layout->addWidget(combo);
  return widget;
}

// ----------------------------------------------------------------------------
indicators::variant_type control_builder_candle_data::get_value(QWidget* widget)
{
  QComboBox* resolution = widget->findChild<QComboBox*>("resolution");
  candle_res res = ohlc_data_resolutions::available_resolutions()[resolution->currentIndex()];
  return indicators::param<candle_data>(widget->objectName(), {res});
}

// ----------------------------------------------------------------------------
// order_book
// ----------------------------------------------------------------------------
QWidget* control_builder_orderbook::build(
    QWidget* parent, QString name, nlohmann::json const& defaults)
{
  QFrame* widget = new QFrame(parent);
  widget->setObjectName(name);
  QVBoxLayout* layout = new QVBoxLayout(widget);
  widget->setLayout(layout);

  // put exchange names into the combox box
  QComboBox* const exchanges = new QComboBox(widget);
  exchanges->setObjectName("exchange");
  layout->addWidget(exchanges);
  if (defaults.contains("exchanges"))
  {
    auto list = to_qstringlist(defaults["exchanges"].get<std::vector<std::string>>());
    exchanges->addItems(list);
    if (defaults.contains("exchange_index"))
    {
      exchanges->setCurrentIndex(defaults["exchange_index"].get<int>());
    }
    // if the choice is only limited to 1 item, no need to show it
    if (list.size() == 1) { exchanges->setVisible(false); }
  }

  // create combo for 1st ticker selection
  std::vector<QComboBox*> qtickers;
  QComboBox* ticker0 = new QComboBox(widget);
  ticker0->setObjectName(fmt::format("ticker-0"));
  layout->addWidget(ticker0);
  qtickers.push_back(ticker0);

  // create combo for 2nd ticker selection
  QComboBox* ticker1 = new QComboBox(widget);
  ticker1->setObjectName(fmt::format("ticker-1"));
  layout->addWidget(ticker1);
  qtickers.push_back(ticker1);

  // callback triggered when exchange combo is modified
  // this lambda will set the ticker combo using the tickers available from the exchanges
  auto set_ticker_strings = [&](int /*index*/) {
    std::string text = exchanges->currentText().toStdString();
    auto ex =
        ranges::find_if(global_settings.networks_, [&](auto e) { return (e->get_name() == text); });
    if (ex == global_settings.networks_.end()) return;
    //
    abstract_exchange::subscription_lock_type l;
    auto tickers = (*ex)->tickers_subscribed(l);
    for (auto combo : qtickers)
    {
      QString currentText = combo->currentText();
      QStringList temp;
      for (auto const& [cp, td] : tickers) { temp << currency_pair_qstring(cp); }
      combo->clear();
      combo->addItems(temp);
      // try to restore active selection if it is still there
      if (currentText != "") combo->setCurrentText(currentText);
    }
  };

  QWidget::connect(exchanges, &QComboBox::currentIndexChanged, widget,
      [=](int index) { set_ticker_strings(index); });

  // fill tickers with user supplied strings if requested
  if (defaults.contains("tickers-0"))
  {
    auto tickers = to_qstringlist(defaults["tickers-0"].get<std::vector<std::string>>());
    widget->setProperty("TSize", QVariant(static_cast<int>(tickers.size())));
    ticker0->addItems(tickers);
  }

  if (defaults.contains("tickers-1"))
  {
    auto tickers = to_qstringlist(defaults["tickers-1"].get<std::vector<std::string>>());
    widget->setProperty("TSize", QVariant(static_cast<int>(tickers.size())));
    ticker1->addItems(tickers);
  }

  // trigger the fill of combo boxes for currency pairs if possible
  // trigger callback to initially setup combo items if user did not supply any
  if (!defaults.contains("tickers-0") && !defaults.contains("tickers-1")) set_ticker_strings(0);

  // if the user has requested initial values, set them up
  if (defaults.contains("tickers-0_index"))
    ticker0->setCurrentIndex(defaults["tickers-0_index"].get<int>());
  if (defaults.contains("tickers-1_index"))
    ticker1->setCurrentIndex(defaults["tickers-1_index"].get<int>());
  //
  if (defaults.contains("tickers-0_value"))
  {
    QString value = to_qstring(defaults["tickers-0_value"].get<std::string>());
    if (ticker0->findText(value) >= 0) ticker0->setCurrentText(value);
  }
  if (defaults.contains("tickers-1_value"))
  {
    QString value = to_qstring(defaults["tickers-1_value"].get<std::string>());
    if (ticker1->findText(value) >= 0) ticker1->setCurrentText(value);
  }

  return widget;
}

// ----------------------------------------------------------------------------
indicators::variant_type control_builder_orderbook::get_value(QWidget* widget)
{
  QFrame* f = dynamic_cast<QFrame*>(widget);
  QComboBox* e = f->findChild<QComboBox*>("exchange");
  std::string exch = e->currentText().toStdString();
  // int num_tickers = f->property("TSize").value<int>();
  //
  currency_pair::list tickers;
  for (int i = 0; i < 2; ++i)
  {
    QComboBox* t = f->findChild<QComboBox*>(fmt::format("ticker-{}", i));
    std::string s = t->currentText().toStdString();
    currency_pair cp = string_to_pair(s, "-");
    tickers.push_back(cp);
  }
  //
  return indicators::param<order_book_param>(widget->objectName(), {exch, tickers});
}
