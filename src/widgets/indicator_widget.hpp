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
#include "indicators/indicator_params.hpp"
#include "indicators/indicator_registry.hpp"
#include "ui_indicator_widget.h"

class indicator_widget : public QWidget
{
  Q_OBJECT

  public:
  indicator_widget(indicators::indicator_vector const& i, std::size_t& index);
  ~indicator_widget();

  // return a variant containing a copy of the selected algorithm
  // including all parameters set by the user in the dialog
  indicators::algorithm_ptr get_algorithm();

  void add_to_dialog(QDialog* dlg);

  QVector<QWidget*>& get_param_widgets() { return param_widgets_; }

  protected:
  void update_parameters();

  private slots:
  void refresh_gui(int index);

  private:
  Ui::indicator_widget ui;
  //
  indicators::indicator_vector const& indicators_;
  std::size_t& index_;
  QVector<QWidget*> param_widgets_;
};
