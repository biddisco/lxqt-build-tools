#pragma once

#include <QAction>
#include <QMainWindow>
//
#include "internet/https-async.hpp"
#include "internet/websocket-ssl.hpp"
//
#include "PriceAndPatternPlot.h"
#include "plots/CombinedPriceVolumeCharts.h"
#include "plots/OrderBookPlot.h"
//
#include "ui_mainwindow.h"

class GroxMainWindow : public QMainWindow
{
    Q_OBJECT

    CombinedPriceVolumeCharts* CombinedPriceVolumeCharts_;
    PriceAndPatternPlot* priceAndPatternPlot_;
    OrderBookPlot* OrderBookPlot_;
    QVector<QwtOHLCSample> ohlc_samples;
    std::vector<double> ohlc_volumes;
    bool repeat_ohlc_;

public:
    explicit GroxMainWindow(QWidget* parent = nullptr);
    ~GroxMainWindow();
    void createActions();
    void createMenus();
    bool eventFilter(QObject* obj, QEvent* event);

    static void new_ticker_data(GroxMainWindow*, std::string&&);
    static void new_order_data(GroxMainWindow*, std::string&&);
    static void rest_api_data(GroxMainWindow*, std::string&&);

    void create_data_dir();
    void read_hdf5();
    void write_hdf5(const QVector<QwtOHLCSample>& samples,
        const std::vector<double>& volume, const uint64_t update = 0);
    void request_new_candlestick_data(uint64_t start_t);
    void validate_ohlc();

    // ----------------------------------------------------------------------------
    void merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples,
        const std::vector<double>& new_ohlc_volumes);

    bool isValidSize(QSize* sizeV)
    {
        if (sizeV->width() < 3 || sizeV->width() > 2000 ||
            sizeV->height() < 3 || sizeV->height() > 2000)
            return false;

        return true;
    }

signals:
    void quitApplication();
    void new_ticker_data_ui(QString);
    void new_order_data_ui(QString);
    void new_order_data_replot();
    void new_ohlc_data_ui();

public slots:
    void appExitCleanupHandler();
    void start_websocket();
    void new_ohlc_data();

private:
    Ui::GroxMainWindow ui;
    QAction* actionQuit;

    // instances we need for websocket connnections
    net::contexts io_contexts;
    std::shared_ptr<net::ws::session> ws_trades;
    std::shared_ptr<net::ws::session> ws_bidask;
    std::shared_ptr<net::https::session> https_rest;
    std::thread websocket_thread;
};

static GroxMainWindow* mainwindow;
