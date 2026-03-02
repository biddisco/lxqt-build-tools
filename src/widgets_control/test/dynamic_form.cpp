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
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
//
#include <nlohmann/json.hpp>
//
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_factory.hpp"
#include "widgets_control/indicator_json.hpp"

// ----------------------------------------------------------------------------
nlohmann::ordered_json get_json_for_person()
{
  return {                   //
      {"name", "string"},    //
      {"age", "int"},
      {"address",
          {
              {"street", "string"},    //
              {"postcode", "int"}      //
          }},
      {"gender", "combo"}};
}

// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  register_control_factories();

  nlohmann::json defaults = {                                                        //
      {"name", {{"placeholder", "Enter full name"}}},                                //
      {"age", {{"min", 0}, {"max", 100}, {"value", 35}}},                            //
      {"address",                                                                    //
          {{"street", {{"placeholder", "Main St"}}},                                 //
              {"postcode", {{"min", 10000}, {"max", 99999}, {"value", 12345}}}}},    //
      {"gender",
          {{"placeholder", "please select one"},
              {"entries", std::vector<std::string>{"Male", "Female", "Non-binary"}},
              {"index", 1}}}};
  std::cout << get_json_for_person().dump(4) << std::endl << defaults.dump(4) << std::endl;
  QWidget* widget = build_control(get_json_for_person(), defaults, control_factory::getInstance());
  widget->setWindowTitle("Dynamic Person Editor");
  widget->show();

  QTimer::singleShot(500, &app, [&app]() {
    QApplication::closeAllWindows();
    app.quit();
  });

  return app.exec();
}
