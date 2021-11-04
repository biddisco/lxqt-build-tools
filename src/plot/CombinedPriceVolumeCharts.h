#ifndef CombinedPriceVolumeCharts_H
#define CombinedPriceVolumeCharts_H

#include <QFrame>
#include <qframe.h>
#include <qwt_plot.h>
//
#include "ohlc_price_plot.hpp"
#include <QGridLayout>

class CombinedPriceVolumeCharts : public QFrame
{
    Q_OBJECT


private:
    ohlc_price_plot *ohlc_price_plot_;
//    VolumePlot *volumePlot_;


public:
    CombinedPriceVolumeCharts(QWidget *, ohlc_dataset_manager *);
    virtual ~CombinedPriceVolumeCharts() {}

//    void populateChartData(const InstrumentSelectionInfoPtr &instrSelInfo);
    ohlc_price_plot *get_ohlc_price_plot() const { return ohlc_price_plot_; }

private Q_SLOTS:
    void scaleDivChanged();

};

#endif // CombinedPriceVolumeCharts_H
