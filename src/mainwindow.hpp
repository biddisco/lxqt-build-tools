#pragma once

#include <QAction>
#include <QWidgetAction>
#include <QMainWindow>
#include <QScrollArea>
#include <QTimer>
//
#include "DockManager.h"
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
#include "src/price_chart_widget.hpp"
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
#include "src/widgets/connection_widget.hpp"
// generated
#include "ui_tabbed_form.h"

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

    // tabbed form with old controls on
    Ui::TabbedForm *tabs_;

    // widgets
    QAction* actionQuit;
    std::shared_ptr<QDockWidget> accounts_dock;
    std::shared_ptr<QDockWidget> orders_dock;
    QScrollArea *accounts_scrollwidget;
    QScrollArea *orders_scrollwidget;

    // data
    ohlc_dataset_manager hdf5_ohlc_;

    // plots
    price_chart_widget *price_plot_;
    OrderBookPlot *obp_;

    // timer for candlestick updates
    QTimer *timer_;
    bool candlestick_update_active_;

    // network/exchanges
    exchange::exchange_vector exchange_list_;
    std::shared_ptr<bitstamp_network> bitstamp_network_;
    std::shared_ptr<xrpl_network> xrpl_network_;
    std::shared_ptr<xrpl_network> xrpl_testnet_;

    // io context for websocket/https requests
    net::contexts io_contexts_;
    std::vector<std::thread> ioc_threads_;
    QVBoxLayout *net_layout_;

    // ads:: The main container for docking
    ads::CDockManager* m_DockManager;

    QAction* SavePerspectiveAction = nullptr;
    QWidgetAction* PerspectiveListAction = nullptr;
    QComboBox* PerspectiveComboBox = nullptr;

public:
    explicit GroxMainWindow(QWidget* parent = nullptr);
    ~GroxMainWindow() override;
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

    void saveConnectionSetups();
    void loadConnectionSetups();

    void stream_process(const QwtOHLCSample &data);
    void start_io_threads(int nthreads);

    void execute_filter();

    void build_connection_gui(exchange *ex);

    void createPerspectiveUi();

private slots:

signals:
    void quitApplication();
    void new_ohlc_data_ui(double);
    void new_ledger_data();
    void restart_candlestick_timer();

public slots:
    void appExitCleanupHandler();
    void restore_dockwindows();
    void new_ohlc_data(double res);
    void execute_xrp();
    void execute_usd();
    void perform_arbitrage();
    void transaction_event();
    void restart_candlestick_timer_event();
    void candlestick_timer_event();
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

    void savePerspective();

};

static GroxMainWindow* mainwindow;
