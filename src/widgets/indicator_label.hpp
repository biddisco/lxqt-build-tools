#pragma once

#include <QLabel>
#include <QWidget>
#include <QString>
#include <QMenu>
#include <QMouseEvent>

class indicator_label : public QLabel
{
    Q_OBJECT

public:
    indicator_label(QWidget* pParent=0) : QLabel(pParent, Qt::WindowFlags()) {};
    indicator_label(const QString& text, QWidget* pParent = 0) : QLabel(text, pParent, Qt::WindowFlags()){};

protected :
    virtual void mouseReleaseEvent ( QMouseEvent * ev ) override {
        QMenu MyMenu(this);
        MyMenu.addActions(this->actions());
        MyMenu.exec(ev->globalPosition().toPoint());
    }
};
