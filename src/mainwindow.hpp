#pragma once

#include <QAction>
#include <QMainWindow>
//
#include <qwt_plot_textlabel.h>
#include <qwt_plot_marker.h>
#include <qwt_text.h>
//
#ifndef Q_MOC_RUN
// MOC chokes on keyword "signals" used by belle
# include "extern/belle/include/belle.hh"
#endif
//
#include "src/internet/https-async.hpp"
#include "src/internet/websocket-ssl.hpp"
//
#include "PriceAndPatternPlot.h"
#include "plots/CombinedPriceVolumeCharts.h"
#include "plots/OrderBookPlot.h"
//
#include "ui_mainwindow.h"
//
#include "settings.hpp"
#include "order_book.hpp"

class GroxMainWindow : public QMainWindow
{
    Q_OBJECT

    CombinedPriceVolumeCharts* CombinedPriceVolumeCharts_;
    PriceAndPatternPlot* priceAndPatternPlot_;
    QVector<QwtOHLCSample> ohlc_samples;
    std::vector<double> ohlc_volumes;
    bool repeat_ohlc_;
    bitstamp_order_book *bistamp_orderbook_;
    xrpl_order_book *ledger_orderbook_;
    QwtPlotTextLabel *timelabel_;
    std::shared_ptr<OrderBookPlot> obp;

//    http::request<http::string_body> bitstamp_request_;

public:
    explicit GroxMainWindow(QWidget* parent = nullptr);
    ~GroxMainWindow();
    void createActions();
    void createMenus();
    bool eventFilter(QObject* obj, QEvent* event);

    static void new_ticker_data(GroxMainWindow*, std::string&&);
    static void new_order_data(GroxMainWindow*, std::string_view);
    static void new_ledger_order_data(GroxMainWindow*, std::string_view);
    //
    void receive_ohlc_data(std::string&&);
    void bitstamp_account_data(std::string&&);
    void ledger_reply(std::string&&);
    void ledger_order_book(bool buy_xrp);

    void create_data_dir();
    void read_hdf5();
    void write_hdf5(const QVector<QwtOHLCSample>& samples,
        const std::vector<double>& volume, const uint64_t update = 0);
    void request_new_candlestick_data(uint64_t start_t=0);
    void validate_ohlc();

    // ----------------------------------------------------------------------------
    void bitstamp_request(const std::string &url_path, const std::string &url_query);
    void update_accounts(app_settings* app_ini);
    void ledger_balance();

    // ----------------------------------------------------------------------------
    void merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples,
        const std::vector<double>& new_ohlc_volumes);

    bool isValidSize(QSize* sizeV)
    {
        if (sizeV->width() < 3 || sizeV->width() > 2000 || sizeV->height() < 3 ||
            sizeV->height() > 2000)
            return false;

        return true;
    }

signals:
    void quitApplication();
    void new_ticker_data_ui(QString);
    void new_order_bitstamp_ui(QString);
    void new_order_xrpl_ui(QString);
    void bitstamp_orderbook_replot();
    void ledger_orderbook_replot();
    void new_ohlc_data_ui();
    void new_ledger_data();

public slots:
    void appExitCleanupHandler();
    void start_websocket();
    void new_ohlc_data();
    void update_accounts();

    // ----------------------------------
    void transfer_setup_xrp(double);
    void transfer_setup_usd(double);
    //
    void xrp_dir_clicked();
    void q1x_clicked();
    void q2x_clicked();
    void q3x_clicked();
    void q4x_clicked();
    //
    void usd_dir_clicked();
    void q1u_clicked();
    void q2u_clicked();
    void q3u_clicked();
    void q4u_clicked();

    void capture_image();

private:
    Ui::GroxMainWindow ui;
    QAction* actionQuit;

    // instances we need for websocket connnections
    net::contexts io_contexts;
    // websocket for bitstamp trade feed
    std::shared_ptr<net::ws::session> ws_trades;
    // websocket for bitstamp bid/ask order book
    std::shared_ptr<net::ws::session> ws_bidask;
    // websocket for ledger order book trades
    std::shared_ptr<net::ws::session> ws_ledger_orderbook;

//    // https session ffor rest API calls
//    std::shared_ptr<net::https::session> https_rest;
    //
    std::thread websocket_thread;

    // An https client object for queuing/dispatching requests
    OB::Belle::Client belle_https_bitstamp;
    OB::Belle::Client belle_https_ripple;
};

static GroxMainWindow* mainwindow;
