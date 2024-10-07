#pragma once

#include <QAbstractTableModel>
#include <QComboBox>
#include <QPushButton>
#include <QTableView>
#include <QWidget>

//
#include "exchange/exchange.hpp"
#include "indicators/indicator_definitions.hpp"
#include "indicators/indicator_types.hpp"
#include "plot/indicator_plot.hpp"
#include "plot/ohlc_picker.hpp"
#include "plot/ohlc_price_plot.hpp"

class ohlc_dataset_view;

namespace Ui {
  class price_chart_widget;
}

// ----------------------------------------------------------------------------
struct indicator_data
{
  QString text;
  QString params;
  //  indicators::indicator_variant indicator;
  indicator_plot* plot;
  std::vector<timebased_data_curve*> curves;
};
Q_DECLARE_METATYPE(indicator_data*)

// ----------------------------------------------------------------------------
class indicators_model : public QAbstractTableModel
{
  Q_OBJECT
  public:
  explicit indicators_model(QObject* parent = nullptr);

  int rowCount(QModelIndex const& parent = QModelIndex()) const override;
  int columnCount(QModelIndex const& parent = QModelIndex()) const override;
  QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
  //
  void dataAdded();
  //    QModelIndex index(int row, int column,
  //                              const QModelIndex &parent = QModelIndex()) const = 0;
  //    QModelIndex parent(const QModelIndex &child) const = 0;

  //    QModelIndex sibling(int row, int column, const QModelIndex &idx) const;
  //    int rowCount(const QModelIndex &parent = QModelIndex()) const = 0;
  //    int columnCount(const QModelIndex &parent = QModelIndex()) const = 0;
  //    bool hasChildren(const QModelIndex &parent = QModelIndex()) const;

  //    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const = 0;

  std::vector<indicator_data> indicators_;
};

// ----------------------------------------------------------------------------
class price_chart_widget : public QWidget
{
  Q_OBJECT

  private:
  Ui::price_chart_widget* ui;
  //
  ohlc_price_plot* crypto_price_plot_;
  std::vector<indicator_plot*> filter_plots_;
  indicator_plot* assets_plot_;
  QPushButton* btn_indicator_;
  //
  std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_;
  std::shared_ptr<exchange> exchange_;
  std::string ticker_string_;

  indicators_model ind_model_;
  QTableView* ind_vis_;

  public:
  price_chart_widget(
      QWidget*, std::shared_ptr<ohlc_dataset_view>, std::shared_ptr<exchange> ex, currency_pair cp);
  ~price_chart_widget();

  void connect_gui();
  void graph_rescale(int range);

  void update_live_data(ohlctv_sample const& new_sample)
  {
    crypto_price_plot_->update_live_data(new_sample);
  }
  void replot() { crypto_price_plot_->replot(); }

  void show_plot_axes();

  std::tuple<indicator_plot*, timebased_data_curve*> add_indicator_plot(QString const& title,
      point_chart_data* data, QColor const& color, indicators::y_limits ylimits = {0.0, 0.0});

  void remove_indicator_plot(indicator_plot* filter_plot, timebased_data_curve* curve);

  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
};
