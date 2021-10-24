#ifndef CombinedPriceVolumeCharts_H
#define CombinedPriceVolumeCharts_H

#include <QFrame>
#include <qframe.h>
#include <qwt_plot.h>
//
#include "CryptoPricePlot.hpp"
#include <QGridLayout>

class CombinedPriceVolumeCharts : public QFrame
{
    Q_OBJECT


private:
    CryptoPricePlot *CryptoPricePlot_;
//    VolumePlot *volumePlot_;


public:
    CombinedPriceVolumeCharts(QWidget *, data_holder *);
    virtual ~CombinedPriceVolumeCharts() {}

//    void populateChartData(const InstrumentSelectionInfoPtr &instrSelInfo);
    CryptoPricePlot *get_CryptoPricePlot() const { return CryptoPricePlot_; }

private Q_SLOTS:
    void scaleDivChanged();

};

#endif // CombinedPriceVolumeCharts_H
