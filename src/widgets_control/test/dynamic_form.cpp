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
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_factory.hpp"

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

  control_factory factory;
  factory.registerBuilder("int", std::make_unique<control_builder_int>());
  factory.registerBuilder("string", std::make_unique<control_builder_string>());
  factory.registerBuilder("combo", std::make_unique<control_builder_combo>());

  nested_control_configs defaultConfigs =                            //
      {                                                              //
          {"name", {{"placeholder", "Enter full name"}}},            //
          {"age", {{"min", 0}, {"max", 100}}},                       //
          {"address.street", {{"placeholder", "Main St"}}},          //
          {"address.postcode", {{"min", 10000}, {"max", 99999}}},    //
          {"gender",
              {
                  {"placeholder", "please select one"},                        //
                  {"entries", QStringList{"Male", "Female", "Non-binary"}},    //
                  {"index", 1}                                                 //
              }}};

  QWidget* form = build_control(get_fields_for_person(), defaultConfigs, factory);
  form->setWindowTitle("Dynamic Person Editor");
  form->show();

  return app.exec();
}
