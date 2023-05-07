#include <QMessageBox>
//
#include "src/widgets/price_chart_widget.hpp"
#include "ui_price_chart_widget.h"
//
#include "src/data/ohlc_dataset_manager.hpp"
#include "src/indicators/indicator_definitiions.hpp"
#include "src/widgets/digital_clock.hpp"
#include "src/widgets/indicator_dialog.hpp"
//
#include "src/print.hpp"
#include "src/settings.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> pplot_dbg("PricePlt");

// ----------------------------------------------------------------------------
price_chart_widget::price_chart_widget(QWidget* parent,
    std::shared_ptr<ohlc_dataset_view> ohlc, std::shared_ptr<exchange> ex,
    std::string ticker)
  : QWidget(parent)
  , ui(new Ui::price_chart_widget)
  , exchange_(ex)
  , hdf5_ohlc_(ohlc)
  , ticker_string_(ticker)
{
    ui->setupUi(this);
    ui->ticker->setText(ticker.data());
    //
    // Create candlestick plot
    //
    crypto_price_plot_ = new ohlc_price_plot(this, hdf5_ohlc_);
    ui->candlestick_layout->addWidget(crypto_price_plot_);

    //
    // Create stream/filters plot
    //
    assets_plot_ = new filter_plot(this);
    ui->filters_layout->addWidget(assets_plot_);
    assets_plot_->setMinimumHeight(128);
    assets_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

    filters_plot_ = new filter_plot(this);
    ui->filters_layout->addWidget(filters_plot_);
    filters_plot_->setMinimumHeight(128);
    filters_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

    QStringList slist("Auto");
    for (const auto& r : ohlc_data_resolutions::available_resolutions())
    {
        slist << r.name_;
    }
    ui->candle_res->addItems(slist);

    indicators_ = new QPushButton(this);
    indicators_->setText("Indicators");
    indicators_->setFlat(true);
    ui->controls_layout->addWidget(indicators_);

    DigitalClock* clock =
        new DigitalClock(this, global_settings()->get_global_clock_timer());
    ui->controls_layout->addWidget(clock);
    //
    connect_gui();
}

// ----------------------------------------------------------------------------
price_chart_widget::~price_chart_widget()
{
    delete ui;
    delete crypto_price_plot_;
    delete filters_plot_;
    delete assets_plot_;
}

// ----------------------------------------------------------------------------
void price_chart_widget::connect_gui()
{
    // Graph resolution buttons
    connect(
        ui->gt_6, &QAbstractButton::clicked, this, [this]() { graph_rescale(-2); },
        Qt::QueuedConnection);
    connect(
        ui->gt_12, &QAbstractButton::clicked, this, [this]() { graph_rescale(-1); },
        Qt::QueuedConnection);
    connect(
        ui->gt_d, &QAbstractButton::clicked, this, [this]() { graph_rescale(0); },
        Qt::QueuedConnection);
    connect(
        ui->gt_w, &QAbstractButton::clicked, this, [this]() { graph_rescale(1); },
        Qt::QueuedConnection);
    connect(
        ui->gt_m, &QAbstractButton::clicked, this, [this]() { graph_rescale(2); },
        Qt::QueuedConnection);
    connect(
        ui->gt_y, &QAbstractButton::clicked, this, [this]() { graph_rescale(3); },
        Qt::QueuedConnection);
    connect(
        ui->gt_a, &QAbstractButton::clicked, this, [this]() { graph_rescale(4); },
        Qt::QueuedConnection);

    connect(
        ui->candle_res, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            if (index > 0)
            {
                double res = ohlc_data_resolutions::available_resolutions()[index - 1];
                crypto_price_plot_->set_auto_candle_resolution(false);
                if (crypto_price_plot_->adjust_candle_size(res))
                {
                    crypto_price_plot_->adjust_data_scaling();
                }
                crypto_price_plot_->replot();
            }
            else
            {
                crypto_price_plot_->set_auto_candle_resolution(true);
                if (crypto_price_plot_->adjust_candle_size(0))
                {
                    crypto_price_plot_->adjust_data_scaling();
                }
                crypto_price_plot_->replot();
            }
        },
        Qt::QueuedConnection);

    connect(
        ui->heikin, QOverload<int>::of(&QCheckBox::stateChanged), this,
        [this](int state) {
            if (state)
            {
                crypto_price_plot_->setMode(ohlc_chart_curve::HeikinAshi);
            }
            else
            {
                crypto_price_plot_->setMode(
                    QwtPlotTradingCurve::SymbolStyle::CandleStick);
            }
        },
        Qt::QueuedConnection);

    connect(
        crypto_price_plot_->get_interactor(), &ohlc_interactor::repair_pressed, this,
        [this](QPointF p) {
            auto crosshairs = crypto_price_plot_->get_crosshairs();
            double msecs = crosshairs->quantize_x_coord(p.x());
            const QDateTime dt = QDateTime::fromMSecsSinceEpoch(msecs);
            QString s = QLocale::system().toString(dt, "dd-MM-yy hh:mm");

            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(
                this, "Confirm", "Delete from " + s, QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes)
            {
                hdf5_ohlc_->truncate_from_time(msecs);
                pplot_dbg<0>.error(str<>("emit update_candlestick_data"));
                // update_candlestick_data();
            }
            else
            {
                pplot_dbg<0>.debug(str<>("Yes *not* clicked"));
            }
        },
        Qt::QueuedConnection);

    connect(
        crypto_price_plot_, &ohlc_price_plot::plotScaleChanged, this,
        [this](double t1, double t2) {
            filters_plot_->update_time_axis(t1, t2);
            assets_plot_->update_time_axis(t1, t2);

            //                axisScaleDraw(QwtPlot::xBottom)->, crypto_price_plot_->axisScaleDraw(QwtPlot::xBottom));
            //    filters_plot_->setAxisScaleEngine(QwtPlot::xBottom, crypto_price_plot_->axisScaleEngine(QwtPlot::xBottom));
        },
        Qt::QueuedConnection);

    //    QAction* pAction1 = new QAction("Moving average", indicators_);
    //    QAction* pAction2 = new QAction("bar", indicators_);
    //    QAction* pAction3 = new QAction("test", indicators_);
    //    indicators_->addAction(pAction1);
    //    indicators_->addAction(pAction2);
    //    indicators_->addAction(pAction3);

    connect(indicators_, &QPushButton::clicked, this, [this](bool b) {
        pplot_dbg<0>.debug(str<>("Indicators"), exchange_->name(), ticker_string_);

        indicator_dialog in_dialog = indicator_dialog();
        auto result = in_dialog.exec();
        if (result == QDialog::Rejected)
            return;
        if (result != QDialog::Accepted)
        {
            int col = 0;
            //            price_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);

            //            filters_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
            //            filters_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

            //            assets_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
            //            assets_plot_->setAxisScale(QwtAxis::YRight, 0, 1);
            return;
        }

        indicator::moving_average ma{};
        ma.generate(hdf5_ohlc_);
    });
    //    connect(pAction2, SIGNAL(triggered()), this, SLOT(onAction2()));
    //    connect(pAction3, SIGNAL(triggered()), this, SLOT(onAction3()));

    filters_plot_->hide();
    assets_plot_->hide();
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void price_chart_widget::graph_rescale(int range)
{
    auto last_time = hdf5_ohlc_->get_last_sample_time(true);
    double t1 = 0, t2 = last_time;
    if (range == -2)
    {
        t1 = last_time - 0.25 * ohlc_data_resolutions::day;
    }
    else if (range == -1)
    {
        t1 = last_time - 0.5 * ohlc_data_resolutions::day;
    }
    else if (range == 0)
    {
        t1 = last_time - 1.0 * ohlc_data_resolutions::day;
    }
    else if (range == 1)
    {
        t1 = last_time - 7 * ohlc_data_resolutions::day;
    }
    else if (range == 2)
    {
        t1 = last_time - 31 * ohlc_data_resolutions::day;
    }
    else if (range == 3)
    {
        t1 = last_time - 365 * ohlc_data_resolutions::day;
    }
    // special case, to extend current view with new data
    else if (range == 100)
    {
        t1 = last_time - 365 * ohlc_data_resolutions::day;
    }
    else
    {
        t1 = hdf5_ohlc_->get_first_sample_time();
    }
    crypto_price_plot_->update_time_axis(t1, t2);
}

// ----------------------------------------------------------------------------
void price_chart_widget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    bool changed = crypto_price_plot_->update_candle_size();
    pplot_dbg<5>.debug(str<>("Resize"), "res changed", changed);
}

// ----------------------------------------------------------------------------
void price_chart_widget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    bool changed = crypto_price_plot_->update_candle_size();
    pplot_dbg<5>.debug(str<>("Show"), "res changed", changed);
}
