#ifndef CombinedPriceVolumeCharts_H
#define CombinedPriceVolumeCharts_H

#include <QFrame>
#include <qframe.h>
#include <qwt_plot.h>
//
#include "PriceAndPatternPlot.h"
#include <QGridLayout>

class CombinedPriceVolumeCharts : public QFrame
{
    Q_OBJECT


private:
    PriceAndPatternPlot *priceAndPatternPlot_;
//    VolumePlot *volumePlot_;


public:
    CombinedPriceVolumeCharts(QWidget * parent = NULL );
    virtual ~CombinedPriceVolumeCharts() {}

//    void populateChartData(const InstrumentSelectionInfoPtr &instrSelInfo);
    PriceAndPatternPlot *priceAndPatternPlot() const { return priceAndPatternPlot_; }

private Q_SLOTS:
    void scaleDivChanged();

};

#endif // CombinedPriceVolumeCharts_H
