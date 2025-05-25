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
#include <QVBoxLayout>
#include <QWidget>
//
#include "currency/currency_pair.hpp"
#include "debug/demangle_helper.hpp"
#include "exchange/bitstamp.hpp"
#include "indicators/indicator_ptr.hpp"
#include "indicators/indicator_registry.hpp"
#include "util/stringutils.hpp"
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_factory.hpp"

// ----------------------------------------------------------------------------
std::list<control_field_data> get_fields_for_indicator(indicators::algorithm_ptr alg)
{
  std::list<control_field_data> fields;
  std::string desc = alg->get_description();

  int nparams = alg->get_params().size();
  // std::visit([](auto const& obj) { return obj.get_params().size(); }, alg);
  for (int i = 0; i < nparams; ++i)
  {
    // extract info from params
    std::visit(
        [&](auto const& v) {
          std::string type = grox::debug::print_type<typeof(v.val_)>();
          fields.push_back({v.name(), to_qstring(type), {}});
        },
        alg->get_params()[i]);
  }
  return fields;
}

// ----------------------------------------------------------------------------
std::list<control_field_data> get_fields_for_person()
{
  return {
      //
      {"name", "string", {}},    //
      {"age", "int", {}},        //
      {"address", "object",      //
          {
              {"street", "string", {}},    //
              {"postcode", "int", {}}      //
          }},                              //
      {"gender", "combo", {}},             //
  };
}

// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

#ifdef TEST_ACTUAL_NETWORK
  // this isn't complete because subscribed tickers is empty by default
  // so gui generation is empty anyway
  QNetworkAccessManager networkmanager;
  global_settings.networkmanager_ = &networkmanager;
  //
  auto bitstamp = std::make_shared<bitstamp_network>();
  for (auto& acct : bitstamp->accounts())
  {
    if (!bitstamp_network::get_pass_authentication(acct))
    {
      std::cout << "Password authentication failed" << std::endl;
      return EXIT_FAILURE;
    }
  }
  global_settings.networks_.push_back(bitstamp);
#endif

  control_factory factory;
  factory.registerBuilder("int", std::make_unique<control_builder_int>());
  factory.registerBuilder("bool", std::make_unique<control_builder_bool>());
  factory.registerBuilder("double", std::make_unique<control_builder_double>());
  factory.registerBuilder("string", std::make_unique<control_builder_string>());
  factory.registerBuilder("combo", std::make_unique<control_builder_combo>());
  factory.registerBuilder("ohlc_modes", std::make_unique<control_builder_ohlc_mode>());
  factory.registerBuilder("order_book_param", std::make_unique<control_builder_orderbook>());

  auto alg = indicators::indicator_registry::find_by_name("Currency-Exchange");
  std::list<control_field_data> fields = get_fields_for_indicator(alg);
  std::cout << to_qstring(fields).toStdString();

  currency_pair cp1{{"XRP"}, {"USD"}};
  currency_pair cp2{{"XRP"}, {"EUR"}};
  currency_pair cp3{{"XRP"}, {"GBP"}};
  nested_control_configs defaultConfigs = {
      {"Order-Book-1",
          {
              {"label", "Currency pairs"},
              {"exchanges", QStringList{"Bitstamp"}},
              {"tickers1",
                  QStringList{currency_pair_qstring(cp1, "-", false),
                      currency_pair_qstring(cp2, "-", false),
                      currency_pair_qstring(cp3, "-", false)}},
              {"tickers1_index", 0},
              {"tickers2",
                  QStringList{currency_pair_qstring(cp1, "-", false),
                      currency_pair_qstring(cp2, "-", false),
                      currency_pair_qstring(cp3, "-", false)}},
              {"tickers2_index", 1},
          }},
      {"Num Spreads", {{"min", 0}, {"max", 100}}}};

  QWidget* form = build_control(fields, defaultConfigs, factory);
  form->setWindowTitle(to_qstring(alg->get_name()));
  form->show();

  return app.exec();
}
