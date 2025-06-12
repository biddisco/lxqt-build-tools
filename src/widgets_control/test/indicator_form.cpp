#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <QApplication>
#include <QComboBox>
#include <QDialog>
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
#include "widgets_control/indicator_json.hpp"
#include "widgets_control/indicator_widget.hpp"

// ----------------------------------------------------------------------------
QWidget* form_for_algorithm(indicators::shared_algorithm alg, nlohmann::json values = {})
{
  control_factory& factory = control_factory::getInstance();
  register_control_factories();

  nlohmann::ordered_json controls = get_json_layout_indicator(alg);
  if (values.size() == 0) values = get_json_values_indicator(alg);
  //
  QWidget* widget = build_control(controls, values, factory);
  widget->setWindowTitle(to_qstring(alg->get_name()));
  return widget;
}

// ----------------------------------------------------------------------------
void execute_dialog(QWidget* form)
{
  QDialog dlg;
  QHBoxLayout* HLayout = new QHBoxLayout(&dlg);
  HLayout->addWidget(form);
  dlg.setLayout(HLayout);
  dlg.exec();
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

  std::size_t index = 0;
  // execute sliding stop algorithm dialog once
  auto alg_0 = indicators::indicator_registry::find_by_name("Trade: Sliding Stop");
  indicators::indicator_vector indicator_vec = {alg_0};
  indicator_widget widget1(indicator_vec, index);
  auto result = widget1.execute_as_dialog();

  // execute a second time tom see if params are persisted
  alg_0 = indicators::indicator_registry::find_by_name("Trade: Sliding Stop");
  indicator_vec = {alg_0};
  indicator_widget widget2 = indicator_widget(indicator_vec, index);
  result = widget2.execute_as_dialog();

  // execute_dialog(form_for_algorithm(alg_0));

  auto alg_1 = indicators::indicator_registry::find_by_name("Currency-Exchange");
  currency_pair cp1{{"XRP"}, {"USD"}};
  currency_pair cp2{{"XRP"}, {"EUR"}};
  currency_pair cp3{{"XRP"}, {"GBP"}};
  std::vector<std::string> cplist = {           //
      currency_pair_string(cp1, "-", false),    //
      currency_pair_string(cp2, "-", false),    //
      currency_pair_string(cp3, "-", false)};
  nlohmann::json defaults = {//
      {"Order-Book-1",
          {
              {"label", "Currency pairs"},
              {"exchanges", std::vector<std::string>{"Fake Exchange"}},
              {"tickers-0", cplist},
              {"tickers-0_index", 0},
              {"tickers-1", cplist},
              {"tickers-1_index", 1},
          }}};
  execute_dialog(form_for_algorithm(alg_1, defaults));

  // return app.exec();
}
