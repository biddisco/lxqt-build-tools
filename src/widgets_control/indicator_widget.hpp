#pragma once

#include <cstddef>
#include <string>
#include <vector>
//
#include <QDialog>
#include <QVector>
#include <QWidget>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_params.hpp"
#include "indicators/indicator_registry.hpp"

namespace Ui {
  class indicator_widget;
}    // namespace Ui

class indicator_widget : public QDialog
{
  Q_OBJECT

  public:
  // create a dialog that has a selection of algorithms to choose from
  indicator_widget(indicators::indicator_vector const* vec, std::size_t& index);
  // create a dialog that only has a single algorithm instantiated
  indicator_widget(indicators::shared_algorithm alg, nlohmann::json values = {});
  // do not allow termporary vectors to be passed in
  indicator_widget(indicators::indicator_vector&& i, std::size_t& index) = delete;
  ~indicator_widget();

  // return a variant containing a copy of the selected algorithm
  // including all parameters set by the user in the dialog
  indicators::shared_algorithm get_algorithm();

  int execute_as_dialog();

  protected:
  void update_parameters();
  void add_indicator_to_dialog(QDialog* dlg);

  private slots:
  void refresh_gui(indicators::shared_algorithm alg, nlohmann::json values = {});

  private:
  Ui::indicator_widget* ui;
  //
  indicators::indicator_vector const* indicators_;
  indicators::shared_algorithm algorithm_;
  QWidget* indicator_widget_;
};
