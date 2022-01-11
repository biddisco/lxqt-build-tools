#pragma once
// Qt
#include <QDialog>
#include <QString>
#include <QLineEdit>
// STL
#include <vector>
#include <string>
//
#include "src/plot/ohlc_chart_data.hpp"
#include "ui_trade_algorithm.h"

class trade_algorithm : public QDialog
{
    Q_OBJECT

public:
    trade_algorithm();
    ~trade_algorithm();
    //
    int algorithm() {
        return ui.algorithm->currentIndex();
    }
    candle_res resolution(int inx) {
        return ohlc_chart_data::get_resolution(
                    ohlc_chart_data::available_resolutions()[combo(inx)->currentIndex()]);
    }
    QComboBox* combo(int inx) const {
        return combos[inx];
    }
    double param(int inx) const {
        return params[inx]->text().toDouble();
    }
    int num_datasets() { return combos.size(); }
    int num_params() { return params.size(); }
private slots:
    void refresh_gui(int index);

private:
    Ui::trade_algorithm ui;
    //
    QVector<QComboBox*> combos;
    QVector<QLineEdit*> params;
};
