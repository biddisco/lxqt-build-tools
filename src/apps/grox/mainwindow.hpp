#pragma once

#include <QAction>
#include <QMainWindow>
#include <QScrollArea>
#include <QShortcut>
#include <QTimer>
#include <QWidgetAction>
//
#include "plot/OrderBookPlot.h"
#include "plot/ohlc_picker.hpp"
#include "plot/ohlc_price_plot.hpp"
//
#include "ui_mainwindow.h"
//
#include "data/order_book.hpp"
#include "exchange/bitstamp.hpp"
#include "exchange/xrpl_network.hpp"
#include "widgets/connection_widget.hpp"

class AdjustingScrollArea : public QScrollArea
{
  bool eventFilter(QObject* obj, QEvent* ev) override
  {
    if (obj == widget() && ev->type() == QEvent::Resize)
    {
      // Essential vvv
      setMaximumWidth(width() - viewport()->width() + widget()->width());
    }
    return QScrollArea::eventFilter(obj, ev);
  }

  public:
  AdjustingScrollArea(QWidget* parent = 0)
    : QScrollArea{parent}
  {
  }
  void setWidget(QWidget* w)
  {
    QScrollArea::setWidget(w);
    // It happens that QScrollArea already filters widget events,
    // but that's an implementation detail that we shouldn't rely on.
    w->installEventFilter(this);
  }
};

class GroxMainWindow : public QMainWindow
{
  Q_OBJECT

  // main form ui
  Ui::GroxMainWindow ui;

  // widgets
  QAction* actionQuit;
  QFrame* orders_frame_;
  QFrame* accounts_frame_;

  // network/exchanges
  exchange::exchange_vector exchange_list_;
  std::shared_ptr<bitstamp_network> bitstamp_network_;
  std::shared_ptr<xrpl_network> xrpl_network_;
  std::shared_ptr<xrpl_network> xrpl_testnet_;

  QTabWidget* net_layout_;

  // menu helpers for docking support
  QMenu* perspectives_menu_;
  //
  QString active_perspective_;
  //
  QShortcut* qs_shutdown_;
  QShortcut* qs_darkmode_;
  QShortcut* qs_password_;
  QShortcut* qs_arbitrage_;
  int dark_mode_;

  std::vector<std::shared_ptr<QDialog>> trade_widgets_;

  public:
  explicit GroxMainWindow(QWidget* parent = nullptr);
  ~GroxMainWindow() override;

  void progress_events(int ms);
  void connect_gui_controls();
  bool eventFilter(QObject* obj, QEvent* event) override;

  void update_balance(std::string_view addr, double oldb, double newb);
  void display_offers();

  void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void saveWindowSettings();
  void loadWindowSettings();

  void saveTrustlines();
  void loadTrustlines();

  void saveConnectionSetups();
  void loadConnectionSetups();

  void stream_process(ohlctv_sample const& data);

  void execute_filter();

  void createPerspectives_Ui();
  void openPerspective(QString const& name);
  void LoadStyleSheet(int dark);

  private slots:

  signals:
  void quitApplication();
  void new_ledger_data();

  public slots:
  void appExitCleanupHandler();
  void execute_xrp();
  void execute_usd();
  void transaction_event();

  // to connect to xrpl ledger signals
  void update_currency_widget(currency_amount*);
  void update_wallet_widget(ledger_wallet*);

  // ----------------------------------
  //    void transfer_setup_xrp(double);
  //    void transfer_setup_usd(double);
  //    void xrp_dir_clicked();
  //    void usd_dir_clicked();
  void capture_image();

  void savePerspective();
};
