#pragma once
// Qt
#include <QDialog>
#include <QString>
#include <QLineEdit>
// STL
#include <vector>
#include <string>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_definitiions.hpp"
#include "ui_indicator_dialog.h"

class indicator_dialog : public QDialog
{
    Q_OBJECT

public:
    indicator_dialog();
    ~indicator_dialog();

    // return a copy of the selected algorithm
    // inclusing all parameters set by the user
    indicator::types get_algorithm() {
        int index = ui.algorithm->currentIndex();
        return indicator::available_indicators[index];
    }

protected:
    void update_parameters();

private slots:
    void refresh_gui(int index);
    void done(int r) override;

private:
    Ui::indicator_dialog ui;
    //
    QVector<QWidget*> params;
};
