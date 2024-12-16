#pragma once

// Qt
#include <QFont>
// Qwt
#include <QwtPlot>
// Grox
#include "data/order_book.hpp"
#include "plot/OrderBookCurve.h"

// ----------------------------------------------------------------------------
class OrderBookPlot : public QwtPlot
{
  Q_OBJECT

  public:
  OrderBookPlot(QWidget* parent, std::shared_ptr<order_book_base> order_book);
  ~OrderBookPlot();
  //
  void clearPlot();

  QFont axis_title_font;

  static int const bid_ask_max = 200;

  // Graph min/max control, 0=primary, 1=secondary
  double prev_xmin[2];
  double prev_xmax[2];
  double prev_ymax[2];

  // data arrays
  double xData[bid_ask_max];
  double yData[bid_ask_max];

  OrderBookCurve* bid_curve_;
  OrderBookCurve* ask_curve_;

  std::shared_ptr<order_book_base> order_book_;

  public Q_SLOTS:
  void update_graph_limits();
  void update_time_and_replot();
  void exportPlot();
  void new_data_event();

  private Q_SLOTS:
  void showItem(QwtPlotItem*, bool on);
};
