#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <vector>
//
#include <QComboBox>
#include <QPushButton>
#include <QTableView>
#include <QWidget>
//
#include "exchange/abstract_exchange.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators_model.hpp"
#include "plot/indicator_plot.hpp"
#include "plot/ohlc_picker.hpp"
#include "plot/ohlc_price_plot.hpp"

class ohlc_dataset_view;

namespace Ui {
  class price_chart_widget;
}

// ----------------------------------------------------------------------------
class price_chart_widget : public QWidget
{
  Q_OBJECT

  private:
  Ui::price_chart_widget* ui;
  //
  ohlc_price_plot* price_plot_;
  std::vector<indicator_plot*> filter_plots_;
  QPushButton* btn_indicator_;
  //
  std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_;
  std::shared_ptr<abstract_exchange> exchange_;
  std::string ticker_string_;

  indicators_model ind_model_;
  QTableView* ind_vis_;

  public:
  price_chart_widget(QWidget*, std::shared_ptr<ohlc_dataset_view>,
      std::shared_ptr<abstract_exchange> ex, currency_pair cp);
  ~price_chart_widget();

  void connect_gui();
  void graph_rescale(int range);

  void update_live_data(ohlctv_sample const& new_sample)
  {
    price_plot_->update_live_data(new_sample);
  }
  void replot() { price_plot_->replot(); }

  void show_plot_axes();

  std::tuple<indicator_plot*, timebased_data_curve*> add_indicator_plot(QString const& title,
      point_chart_data* data, QColor const& color, indicators::y_limits ylimits = {0.0, 0.0});

  void remove_indicator_plot(QwtPlot* plot, QwtPlotCurve* curve);

  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
};
