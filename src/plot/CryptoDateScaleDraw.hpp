#pragma once

#include <QwtDateScaleDraw>

class CryptoDateScaleDraw : public QwtDateScaleDraw
{
private:
    //

public:
    CryptoDateScaleDraw(Qt::TimeSpec);
    //
    virtual QwtText label(double) const override;
};
