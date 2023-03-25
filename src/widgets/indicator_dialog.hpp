#pragma once
// Qt
#include <QDialog>
#include <QString>
#include <QLineEdit>
// STL
#include <vector>
#include <string>
//
#include "src/data/ohlc_data_resolutions.hpp"
#include "ui_indicator_dialog.h"

class indicator_dialog : public QDialog
{
    Q_OBJECT

public:
    indicator_dialog();
    ~indicator_dialog();
    //
    int algorithm() {
        return ui.algorithm->currentIndex();
    }
    candle_res resolution(int inx) {
        return ohlc_data_resolutions::get_resolution(
                    ohlc_data_resolutions::available_resolutions()[combo(inx)->currentIndex()]);
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
    Ui::indicator_dialog ui;
    //
    QVector<QComboBox*> combos;
    QVector<QLineEdit*> params;
};
