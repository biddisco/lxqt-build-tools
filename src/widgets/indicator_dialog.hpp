#pragma once
// Qt
#include <QDialog>
#include <QLineEdit>
#include <QString>
// STL
#include <string>
#include <vector>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_definitions.hpp"
#include "ui_indicator_dialog.h"

class indicator_dialog : public QDialog
{
  Q_OBJECT

  public:
  indicator_dialog();
  ~indicator_dialog();

  // return a variant containing a copy of the selected algorithm
  // including all parameters set by the user in the dialog
  indicators::indicator_variant get_algorithm()
  {
    int index = ui.algorithm->currentIndex();
    return indicators::available_indicators[index];
  }

  protected:
  void update_parameters();

  private slots:
  void refresh_gui(int index);
  void done(int r) override;

  private:
  Ui::indicator_dialog ui;
  //
  QVector<QWidget*> params;
};
