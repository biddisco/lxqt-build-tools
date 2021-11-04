#include <QGridLayout>
#include <QDebug>
#include <QLayout>
//
#include <qwt_plot.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_draw.h>
//
#include "src/plot/CombinedPriceVolumeCharts.h"
#include "ohlc_price_plot.hpp"

CombinedPriceVolumeCharts::CombinedPriceVolumeCharts(QWidget *parent, ohlc_dataset_manager *data)
    : QFrame( parent )
{
    QGridLayout *layout = new QGridLayout( this );

    layout->setSpacing(0);
    layout->setMargin(4);

    ohlc_price_plot_ = new ohlc_price_plot(this, data);
    layout->addWidget( ohlc_price_plot_,0,0 );

//    volumePlot_ = new VolumePlot(this);
//    layout->addWidget( volumePlot_, 1, 0 );

    layout->setRowStretch(0,80);
    layout->setRowStretch(1,20);

    setObjectName("CombinedPriceVolumeCharts");
    setStyleSheet("#CombinedPriceVolumeCharts { border: 2px solid gray; }");

    connect( ohlc_price_plot_->axisWidget( QwtPlot::xBottom ),
        SIGNAL( scaleDivChanged() ), SLOT( scaleDivChanged() ) );
}

//void CombinedPriceVolumeCharts::populateChartData(const InstrumentSelectionInfoPtr &instrSelInfo)
//{
//    volumePlot_->populateChartData(instrSelInfo);
//    ohlc_price_plot_->populateChartData(instrSelInfo);

//    QwtScaleWidget *priceScaleWidget = ohlc_price_plot_->axisWidget( QwtPlot::yLeft );
//    QwtScaleDraw *priceScaleDraw = priceScaleWidget->scaleDraw();
//    priceScaleDraw->setMinimumExtent( 0.0 );
//    double priceExtent = priceScaleDraw->extent( priceScaleWidget->font() );

//    QwtScaleWidget *volScaleWidget = volumePlot_->axisWidget( QwtPlot::yLeft );
//    QwtScaleDraw *volScaleDraw = volScaleWidget->scaleDraw();
//    volScaleDraw->setMinimumExtent( 0.0 );
//    double volExtent = volScaleDraw->extent( volScaleWidget->font() );

//    double maxExtent = priceExtent>volExtent?priceExtent:volExtent;

//    volScaleDraw->setMinimumExtent(maxExtent);
//    priceScaleDraw->setMinimumExtent(maxExtent);

//    scaleDivChanged();
//}


void CombinedPriceVolumeCharts::scaleDivChanged()
{
//    qDebug() << "Stacked Stock Charts: scaleDivChange()";
//    volumePlot_->rescaleAxis(ohlc_price_plot_->axisScaleDiv( QwtPlot::xBottom ));
}
