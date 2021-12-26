#ifndef XRP_FUNCTIONS_H
#define XRP_FUNCTIONS_H

#include <QWidget>
//
#include "src/settings.hpp"
class xrpl_network;

namespace Ui {
class xrp_functions;
}

class xrp_functions : public QWidget
{
    Q_OBJECT

private:
    Ui::xrp_functions *ui;
    currency       currency_;
    basic_account *account_;
    xrpl_network  *network_;
    double         amount_;

public:
    xrp_functions(xrpl_network * network, basic_account *account, QWidget *parent = nullptr);
    ~xrp_functions();

public slots:
    void exec_trustline();
    // ----------------------------------
};

#endif // XRP_FUNCTIONS_H
