#pragma once

#include <QAction>
#include <QMainWindow>
#include <QScrollArea>
#include <QTimer>
//
#ifndef Q_MOC_RUN
// MOC chokes on keyword "signals" used by belle
# include "extern/belle/include/belle.hh"
#endif
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
//
#include "src/plot/ohlc_price_plot.hpp"
#include "src/plot/ohlc_picker.hpp"
#include "src/plot/OrderBookPlot.h"
#include "src/plot/filter_plot.hpp"
//
#include "src/data/ohlc_dataset_manager.hpp"
//
#include "ui_mainwindow.h"
//
#include "exchange/bitstamp.hpp"
#include "src/exchange/xrpl_network.hpp"
#include "src/settings.hpp"
#include "src/order_book.hpp"
#include "src/stream/trade_filter.hpp"

class AdjustingScrollArea : public QScrollArea {
   bool eventFilter(QObject * obj, QEvent * ev) override {
      if (obj == widget() && ev->type() == QEvent::Resize) {
         // Essential vvv
         setMaximumWidth(width() - viewport()->width() + widget()->width());
      }
      return QScrollArea::eventFilter(obj, ev);
   }
public:
   AdjustingScrollArea(QWidget * parent = 0) : QScrollArea{parent} {}
   void setWidget(QWidget *w) {
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
    std::shared_ptr<QDockWidget> accounts_dock;
    std::shared_ptr<QDockWidget> orders_dock;
    QScrollArea *accounts_scrollwidget;
    QScrollArea *orders_scrollwidget;

    // data
    ohlc_dataset_manager hdf5_ohlc_;

    // plots
    ohlc_price_plot* crypto_price_plot_;
    filter_plot *filters_plot_;
    filter_plot *assets_plot_;
    OrderBookPlot *obp_;
    QTimer *timer_;

    // network/exchanges
    std::shared_ptr<bitstamp_network> bitstamp_network_;
    std::shared_ptr<xrpl_network> xrpl_network_;
    std::shared_ptr<xrpl_network> xrpl_testnet_;


    // io context for websocket/https requests
    net::contexts io_contexts;
    std::vector<std::thread> ioc_threads_;

public:
    explicit GroxMainWindow(QWidget* parent = nullptr);
    ~GroxMainWindow();
    void createActions();
    void createMenus();
    bool eventFilter(QObject* obj, QEvent* event) override;

    //
    void receive_ohlc_data(std::string&&);

    void update_candlestick_data();

    void update_balance(std::string_view addr, double oldb, double newb);
    void display_offers();

    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void saveWindowSettings();
    void loadWindowSettings();

    void saveTrustlines();
    void loadTrustlines();

    void stream_process(const QwtOHLCSample &data);
    void start_io_threads(int nthreads);

    void execute_filter();

signals:
    void quitApplication();
    void new_ohlc_data_ui();
    void new_ledger_data();

public slots:
    void appExitCleanupHandler();
    void start_websocket();
    void restore_dockwindows();
    void new_ohlc_data();
    void update_account_balances();
    void execute_xrp();
    void execute_usd();
    void perform_arbitrage();
    void transaction_event();
    void on_timer();
    void orderbook_text_update();

    // to connect to xrpl ledger signals
    void update_currency_widget(currency*);
    void update_wallet_widget(ledger_wallet*);

    // ----------------------------------
//    void transfer_setup_xrp(double);
//    void transfer_setup_usd(double);
//    void xrp_dir_clicked();
//    void usd_dir_clicked();
    void capture_image();

    void graph_rescale(int range);
};

static GroxMainWindow* mainwindow;
