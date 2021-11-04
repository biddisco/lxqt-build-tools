#pragma once

#include <QwtDateScaleDraw>

class ohlc_date_scaledraw : public QwtDateScaleDraw
{
public:
    ohlc_date_scaledraw(Qt::TimeSpec);
    //
    virtual QwtText label(double) const override;
};
