#include <QMouseEvent>
#include <QDebug>
//
#include "src/plot/PlotInteractor.hpp"
#include "src/plot/CryptoPricePlot.hpp"

#include <QwtScaleMap>
#include <QwtScaleDiv>
//
#include "qwt_panner.h"
#include "qwt_picker.h"
#include "qwt_painter.h"

#include <qpainter.h>
#include <qpixmap.h>
#include <qevent.h>
#include <qcursor.h>
#include <qbitmap.h>

class PlotInteractor::PrivateData
{
  public:
    PrivateData()
        : button( Qt::LeftButton )
        , buttonModifiers( Qt::NoModifier )
        , abortKey( Qt::Key_Escape )
        , abortKeyModifiers( Qt::NoModifier )
        , isEnabled( false )
        , plot(nullptr)
    {
        for ( int axis = 0; axis < QwtAxis::AxisPositions; axis++ )
            isAxisEnabled[axis] = true;
    }

    ~PrivateData() {}

    bool isAxisEnabled[QwtAxis::AxisPositions];

    Qt::MouseButton button;
    Qt::KeyboardModifiers buttonModifiers;

    int abortKey;
    Qt::KeyboardModifiers abortKeyModifiers;

    QPoint initialPos;
    QPoint pos;

    bool isEnabled;

    CryptoPricePlot* plot;
};

PlotInteractor::PlotInteractor(CryptoPricePlot* parent, data_holder *data)
    : QObject( parent )
    , data_holder_(data)
{
    m_data = new PrivateData();

    setEnabled( true );

    connect( this, SIGNAL(panned(int,int)),
        SLOT(panCanvas(int,int)) );
    connect( this, SIGNAL(zoomed(int,int)),
        SLOT(zoomCanvas(int,int)) );
}

//! Destructor
PlotInteractor::~PlotInteractor()
{
    delete m_data;
}

//! \return Parent widget, where the rescaling happens
QWidget* PlotInteractor::parentWidget()
{
    return qobject_cast< QWidget* >( parent() );
}

//! \return Parent widget, where the rescaling happens
const QWidget* PlotInteractor::parentWidget() const
{
    return qobject_cast< const QWidget* >( parent() );
}


// ----------------------------------------------------------------------------
//! Return plot widget, containing the observed plot canvas
CryptoPricePlot* PlotInteractor::plot()
{
    QWidget* w = parentWidget();
    return qobject_cast< CryptoPricePlot* >( w );
}

// ----------------------------------------------------------------------------
//! Return plot widget, containing the observed plot canvas
const CryptoPricePlot* PlotInteractor::plot() const
{
    const QWidget* w = parentWidget();
    return qobject_cast< const CryptoPricePlot* >( w );
}

/*!
   \brief En/Disable an axis

   Axes that are enabled will be synchronized to the
   result of panning. All other axes will remain unchanged.

   \param axisId Axis id
   \param on On/Off

   \sa isAxisEnabled(), moveCanvas()
 */
void PlotInteractor::setAxisEnabled( QwtAxisId axisId, bool on )
{
    if ( QwtAxis::isValid( axisId ) )
        m_data->isAxisEnabled[axisId] = on;
}

/*!
   Test if an axis is enabled

   \param axisId Axis
   \return True, if the axis is enabled

   \sa setAxisEnabled(), moveCanvas()
 */
bool PlotInteractor::isAxisEnabled( QwtAxisId axisId ) const
{
    if ( QwtAxis::isValid( axisId ) )
        return m_data->isAxisEnabled[axisId];

    return true;
}


/*!
   Adjust the enabled axes according to dx/dy

   \param dx Pixel offset in x direction
   \param dy Pixel offset in y direction

   \sa PlotInteractor::panned()
 */

void PlotInteractor::panCanvas( int dx, int dy )
{
    if ( dx == 0 && dy == 0 )
        return;

    QwtPlot* plot = this->plot();
    if ( plot == NULL )
        return;

    const bool doAutoReplot = plot->autoReplot();
    plot->setAutoReplot( false );

    // because we rescale the YAxis as the XAxis moves, we traverse the axes
    // in order with X first, so that once those bounds have updated, we can
    // use them to do Y
    auto axes = {QwtAxis::XBottom, QwtAxis::YLeft, QwtAxis::YRight };

    double new_xmin, new_xmax;
    for (auto axisPos : axes)
    {
        const QwtAxisId axisId( axisPos );

        if ( !m_data->isAxisEnabled[axisId] )
            continue;

        // get the pixel/plot coordinate transform
        const QwtScaleMap map = plot->canvasMap( axisId );

        // get the current min/max
        const double p1 = map.transform( plot->axisScaleDiv( axisId ).lowerBound() );
        const double p2 = map.transform( plot->axisScaleDiv( axisId ).upperBound() );

        double d1, d2;
        if ( QwtAxis::isXAxis( axisPos ) )
        {
            d1 = map.invTransform( p1 - dx );
            d2 = map.invTransform( p2 - dx );
            new_xmin = d1;
            new_xmax = d2;
        }
        else
        {
            const auto minmax = data_holder_->get_min_max_window(new_xmin, new_xmax, 0.05);
            d1 = minmax.minval_;
            d2 = minmax.maxval_;
        }

        plot->setAxisScale( axisId, d1, d2 );
    }

    plot->setAutoReplot( doAutoReplot );
    plot->replot();
}

void PlotInteractor::zoomCanvas( int dx, int dy )
{
    if ( dx == 0 && dy == 0 )
        return;

    CryptoPricePlot* plot = this->plot();
    if ( plot == NULL )
        return;

    const bool doAutoReplot = plot->autoReplot();
    plot->setAutoReplot( false );

    // because we rescale the YAxis as the XAxis moves, we traverse the axes
    // in order with X first, so that once those bounds have updated, we can
    // use them to do Y
    auto axes = {QwtAxis::XBottom, QwtAxis::YLeft, QwtAxis::YRight };

    double new_xmin, new_xmax;
    for (auto axisPos : axes)
    {
        const QwtAxisId axisId( axisPos );
        if ( !m_data->isAxisEnabled[axisId] )
            continue;

        // get the pixel/plot coordinate transform
        const QwtScaleMap map = plot->canvasMap( axisId );

        // get the current min in pixel coords
        const double p1 = map.transform( plot->axisScaleDiv( axisId ).lowerBound() );
        // get the current max in world coords
        double d2 = plot->axisScaleDiv( axisId ).upperBound();

        // transform new pixel range back to world coords
        // leave maxbound unchanged, but zoom by extending lowerbound
        double d1;
        if ( QwtAxis::isXAxis( axisPos ) )
        {
            d1 = map.invTransform( p1 + dy );
            new_xmin = d1;
            new_xmax = d2;
        }
        else
        {
            const auto minmax = data_holder_->get_min_max_window(new_xmin, new_xmax, 0.05);
            d1 = minmax.minval_;
            d2 = minmax.maxval_;
        }

        plot->setAxisScale( axisId, d1, d2 );
    }

    plot->adjust_candle_size();
    plot->setAutoReplot( doAutoReplot );
    plot->replot();
}

// ----------------------------------------------------------------------------
bool PlotInteractor::eventFilter( QObject * object, QEvent * event)
{
    if ( object == NULL || object != parentWidget() )
            return false;

    switch ( event->type() )
    {
        // 2 finger trackpad movements appear as scroll events
        case QEvent::Wheel:
        {
            QWheelEvent* we = static_cast<QWheelEvent*>(event);
            auto d = we->angleDelta();
            // sideways swipe
            if (std::abs(d.x()) >= std::abs(d.y())) {
                Q_EMIT panned( d.x()/2, d.y()/2 );
            }
            // vertical swipe
            else {
                Q_EMIT zoomed( d.x()/2, d.y()/2 );
            }
            break;
        }
        case QEvent::MouseButtonPress:
        {
            widgetMousePressEvent( static_cast<QMouseEvent *>( event ) );
            break;
        }
        case QEvent::MouseMove:
        {
            break;
            QMouseEvent * evr = static_cast<QMouseEvent *>( event );
            widgetMouseMoveEvent( evr );
            widgetMouseReleaseEvent( evr  );
            setMouseButton(evr->button(), evr->modifiers());
            widgetMousePressEvent( evr);
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            QMouseEvent * evr = static_cast<QMouseEvent *>( event );
            widgetMouseReleaseEvent( static_cast<QMouseEvent *>( event ) );
            break;
        }
        case QEvent::KeyPress:
        {
            widgetKeyPressEvent( static_cast<QKeyEvent *>( event ) );
            break;
        }
        case QEvent::KeyRelease:
        {
            widgetKeyReleaseEvent( static_cast<QKeyEvent *>( event ) );
            break;
        }
        case QEvent::Paint:
        {
            if ( parentWidget()->isVisible() )
                return true;
            break;
        }

        case QEvent::NativeGesture:
        {
            qreal value = static_cast<QNativeGestureEvent*>(event)->value();

            if (value > 0) {
                 qDebug() << static_cast<QNativeGestureEvent*>(event)->value();
            }
            else if (value < 0) {
                 qDebug() << static_cast<QNativeGestureEvent*>(event)->value();            }
            break;
        }
        default:;
    }

    return false;
}


/*!
   Change the mouse button and modifiers used for panning
   The defaults are Qt::LeftButton and Qt::NoModifier
 */
void PlotInteractor::setMouseButton( Qt::MouseButton button,
    Qt::KeyboardModifiers modifiers )
{
    m_data->button = button;
    m_data->buttonModifiers = modifiers;
}

//! Get mouse button and modifiers used for panning
void PlotInteractor::getMouseButton( Qt::MouseButton& button,
    Qt::KeyboardModifiers& modifiers ) const
{
    button = m_data->button;
    modifiers = m_data->buttonModifiers;
}

/*!
   Change the abort key
   The defaults are Qt::Key_Escape and Qt::NoModifiers

   \param key Key ( See Qt::Keycode )
   \param modifiers Keyboard modifiers
 */
void PlotInteractor::setAbortKey( int key,
    Qt::KeyboardModifiers modifiers )
{
    m_data->abortKey = key;
    m_data->abortKeyModifiers = modifiers;
}

//! Get the abort key and modifiers
void PlotInteractor::getAbortKey( int& key,
    Qt::KeyboardModifiers& modifiers ) const
{
    key = m_data->abortKey;
    modifiers = m_data->abortKeyModifiers;
}


/*!
   \brief En/disable the panner

   When enabled is true an event filter is installed for
   the observed widget, otherwise the event filter is removed.

   \param on true or false
   \sa isEnabled(), eventFilter()
 */
void PlotInteractor::setEnabled( bool on )
{
    if ( m_data->isEnabled != on )
    {
        m_data->isEnabled = on;

        QWidget* w = parentWidget();
        if ( w )
        {
            if ( m_data->isEnabled )
            {
                w->installEventFilter( this );
            }
            else
            {
                w->removeEventFilter( this );
            }
        }
    }
}

/*!
   \return true when enabled, false otherwise
   \sa setEnabled, eventFilter()
 */
bool PlotInteractor::isEnabled() const
{
    return m_data->isEnabled;
}


void PlotInteractor::widgetMouseInitEvent( QMouseEvent* mouseEvent)
{
    m_data->initialPos = m_data->pos = mouseEvent->pos();
}

/*!
   Handle a mouse press event for the observed widget.

   \param mouseEvent Mouse event
   \sa eventFilter(), widgetMouseReleaseEvent(),
      widgetMouseMoveEvent(),
 */
void PlotInteractor::widgetMousePressEvent( QMouseEvent* mouseEvent )
{
    m_data->initialPos = m_data->pos = mouseEvent->pos();
}

/*!
   Handle a mouse move event for the observed widget.

   \param mouseEvent Mouse event
   \sa eventFilter(), widgetMousePressEvent(), widgetMouseReleaseEvent()
 */
void PlotInteractor::widgetMouseMoveEvent( QMouseEvent* mouseEvent )
{
    if ( !parentWidget()->isVisible() )
        return;

    QPoint pos = mouseEvent->pos();
    if (pos != m_data->pos)
    {
        m_data->pos = pos;
//        parentWidget()->update();

        Q_EMIT moved( m_data->pos.x() - m_data->initialPos.x(),
            m_data->pos.y() - m_data->initialPos.y() );
    }
}

/*!
   Handle a mouse release event for the observed widget.

   \param mouseEvent Mouse event
   \sa eventFilter(), widgetMousePressEvent(),
      widgetMouseMoveEvent(),
 */
void PlotInteractor::widgetMouseReleaseEvent( QMouseEvent* mouseEvent )
{
    if ( parentWidget()->isVisible() )
    {
        QPoint pos = mouseEvent->pos();

        m_data->pos = pos;

        if ( m_data->pos != m_data->initialPos )
        {
        }
    }
}

/*!
   Handle a key press event for the observed widget.

   \param keyEvent Key event
   \sa eventFilter(), widgetKeyReleaseEvent()
 */
void PlotInteractor::widgetKeyPressEvent( QKeyEvent* keyEvent )
{
    if ( ( keyEvent->key() == m_data->abortKey )
        && ( keyEvent->modifiers() == m_data->abortKeyModifiers ) )
    {
    }
}

/*!
   Handle a key release event for the observed widget.

   \param keyEvent Key event
   \sa eventFilter(), widgetKeyReleaseEvent()
 */
void PlotInteractor::widgetKeyReleaseEvent( QKeyEvent* keyEvent )
{
    Q_UNUSED( keyEvent );
}

