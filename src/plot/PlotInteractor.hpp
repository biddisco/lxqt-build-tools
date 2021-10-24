#pragma once

#include <QWidget>
#include <QwtAxis>
#include <QwtAxisId>
//
#include <QwtGlobal>
//
#include "src/plot/PlotInteractor.hpp"
#include "src/data/data_holder.hpp"

class CryptoPricePlot;
class QCursor;
class QPixmap;

class QWT_EXPORT PlotInteractor : public QObject
{
    Q_OBJECT

public:
    explicit PlotInteractor(CryptoPricePlot* plot, data_holder *data);
    virtual ~PlotInteractor();

    QWidget* parentWidget();
    const QWidget* parentWidget() const;
    //
    CryptoPricePlot* plot();
    const CryptoPricePlot* plot() const;

    void setAxisEnabled( QwtAxisId axisId, bool on );
    bool isAxisEnabled( QwtAxisId ) const;

public:
    void setEnabled( bool );
    bool isEnabled() const;

    void setMouseButton( Qt::MouseButton,
                         Qt::KeyboardModifiers = Qt::NoModifier );
    void getMouseButton( Qt::MouseButton& button,
                         Qt::KeyboardModifiers& ) const;

    void setAbortKey( int key, Qt::KeyboardModifiers = Qt::NoModifier );
    void getAbortKey( int& key, Qt::KeyboardModifiers& ) const;

    void setCursor( const QCursor& );
    const QCursor cursor() const;

    void setOrientations( Qt::Orientations );
    Qt::Orientations orientations() const;

    bool isOrientationEnabled( Qt::Orientation ) const;

    virtual bool eventFilter( QObject*, QEvent* ) QWT_OVERRIDE;

public Q_SLOTS:
    void panCanvas( int dx, int dy );
    void zoomCanvas( int dx, int dy );

Q_SIGNALS:
    /*!
       Signal emitted, when panning is done

       \param dx Offset in horizontal direction
       \param dy Offset in vertical direction
     */
    void panned( int dx, int dy );
    void zoomed( int dx, int dy );

    /*!
       Signal emitted, while the widget moved, but panning
       is not finished.

       \param dx Offset in horizontal direction
       \param dy Offset in vertical direction
     */
    void moved( int dx, int dy );

protected:
    virtual void widgetMouseInitEvent( QMouseEvent* );
    virtual void widgetMousePressEvent( QMouseEvent* );
    virtual void widgetMouseReleaseEvent( QMouseEvent* );
    virtual void widgetMouseMoveEvent( QMouseEvent* );
    virtual void widgetKeyPressEvent( QKeyEvent* );
    virtual void widgetKeyReleaseEvent( QKeyEvent* );

private:

    class PrivateData;
    PrivateData* m_data;

protected:
    data_holder *data_holder_;
};
