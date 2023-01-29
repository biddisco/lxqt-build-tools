#ifndef PRICE_CHART_WIDGET_H
#define PRICE_CHART_WIDGET_H

#include <QWidget>
//
#include "src/plot/ohlc_price_plot.hpp"
#include "src/plot/ohlc_picker.hpp"
#include "src/plot/filter_plot.hpp"
#include "src/exchange/exchange.hpp"

class ohlc_dataset_view;

namespace Ui {
class price_chart_widget;
}

class price_chart_widget : public QWidget
{
    Q_OBJECT

private:
    Ui::price_chart_widget *ui;
    //
    ohlc_price_plot* crypto_price_plot_;
    filter_plot *filters_plot_;
    filter_plot *assets_plot_;
    //
    std::shared_ptr<exchange> exchange_;
    std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_;

public:
    price_chart_widget(QWidget *, std::shared_ptr<ohlc_dataset_view>, std::shared_ptr<exchange> ex, std::string ticker);
    ~price_chart_widget();

    void connect_gui();
    void graph_rescale(int range);

    void update_live_data(QwtOHLCSample const &new_sample)
    {
        crypto_price_plot_->update_live_data(new_sample);
    }
    void replot()
    {
        crypto_price_plot_->replot();
    }

    void resizeEvent(QResizeEvent *event) override;
};

#endif // PRICE_CHART_WIDGET_H
