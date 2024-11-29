// STL
#include <iomanip>
#include <iostream>
// Qt
#include <QMouseEvent>
// Qwt
#include <QwtAxis>
#include <QwtPlot>
#include <QwtScaleEngine>
#include <QwtScaleMap>
// Grox
#include "debug/print.hpp"
#include "plot/ohlc_interactor.hpp"
#include "plot/timebased_chart_plot.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
inline constexpr print_threshold<Level, debug_level> inter_dbg("interact");

// ----------------------------------------------------------------------------
class ohlc_interactor::PrivateData
{
  public:
  PrivateData()
    : button(Qt::LeftButton)
    , buttonModifiers(Qt::NoModifier)
    , abortKey(Qt::Key_Escape)
    , abortKeyModifiers(Qt::NoModifier)
    , isEnabled(false)
    , plot(nullptr)
  {
    for (int axis = 0; axis < QwtAxis::AxisPositions; axis++) isAxisEnabled[axis] = true;
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

  timebased_chart_plot* plot;
};

// ----------------------------------------------------------------------------
ohlc_interactor::ohlc_interactor(timebased_chart_plot* parent)
  : QObject(parent)
{
  m_data = new PrivateData();

  setEnabled(true);

  connect(this, SIGNAL(panned(int, int)), SLOT(panCanvas(int, int)));
  connect(this, SIGNAL(zoomed(int, int)), SLOT(zoomCanvas(int, int)));
}

//! Destructor
ohlc_interactor::~ohlc_interactor() { delete m_data; }

//! \return Parent widget, where the rescaling happens
QWidget* ohlc_interactor::parentWidget() { return qobject_cast<QWidget*>(parent()); }

//! \return Parent widget, where the rescaling happens
QWidget const* ohlc_interactor::parentWidget() const
{
  return qobject_cast<QWidget const*>(parent());
}

// ----------------------------------------------------------------------------
//! Return plot widget, containing the observed plot canvas
timebased_chart_plot* ohlc_interactor::plot()
{
  QWidget* w = parentWidget();
  return qobject_cast<timebased_chart_plot*>(w);
}

// ----------------------------------------------------------------------------
//! Return plot widget, containing the observed plot canvas
timebased_chart_plot const* ohlc_interactor::plot() const
{
  QWidget const* w = parentWidget();
  return qobject_cast<timebased_chart_plot const*>(w);
}

// ----------------------------------------------------------------------------
/*!
   \brief En/Disable an axis

   Axes that are enabled will be synchronized to the
   result of panning. All other axes will remain unchanged.

   \param axisId Axis id
   \param on On/Off

   \sa isAxisEnabled(), moveCanvas()
 */
void ohlc_interactor::setAxisEnabled(QwtAxisId axisId, bool on)
{
  if (QwtAxis::isValid(axisId)) m_data->isAxisEnabled[axisId] = on;
}

// ----------------------------------------------------------------------------
/*!
   Test if an axis is enabled

   \param axisId Axis
   \return True, if the axis is enabled

   \sa setAxisEnabled(), moveCanvas()
 */
bool ohlc_interactor::isAxisEnabled(QwtAxisId axisId) const
{
  if (QwtAxis::isValid(axisId)) return m_data->isAxisEnabled[axisId];

  return true;
}

// ----------------------------------------------------------------------------
void ohlc_interactor::panCanvas(int dx, int dy)
{
  if (dx == 0 && dy == 0) return;
  timebased_chart_plot* plot = this->plot();
  if (plot == NULL) return;

  // get the X axis pixel/plot coordinate transform
  QwtScaleMap const map = plot->canvasMap(QwtAxis::XBottom);

  // get the X axis extent, transform it into pixels
  double const p1 = map.transform(plot->axisScaleDiv(QwtAxis::XBottom).lowerBound());
  double const p2 = map.transform(plot->axisScaleDiv(QwtAxis::XBottom).upperBound());
  // slide it left or right by dx amount
  double t1 = map.invTransform(p1 - dx);
  double t2 = map.invTransform(p2 - dx);

  plot->update_time_axis(t1, t2, true);
}

// ----------------------------------------------------------------------------
void ohlc_interactor::zoomCanvas(int dx, int dy)
{
  if (dx == 0 && dy == 0) return;
  timebased_chart_plot* plot = this->plot();
  if (plot == NULL) return;

  // get the X axis pixel/plot coordinate transform
  QwtScaleMap const map = plot->canvasMap(QwtAxis::XBottom);

  // we zoom keeping the point under the mouse at the same position in X
  // so we do not simply add an amount to both ends, but compute a more complex
  // transformation

  // x min/max in world coords
  double x1 = plot->axisScaleDiv(QwtAxis::XBottom).lowerBound();
  double x2 = plot->axisScaleDiv(QwtAxis::XBottom).upperBound();
  double xd = x2 - x1;
  // mouse pos in world coords
  double xm = map.invTransform(m_data->initialPos.x());

  // the amount we are going to zoom by depends on wheel amount
  double p1 = map.transform(x1);
  double xy = map.invTransform(p1 - dy) - x1;

  double t2 = (x2 * xd + x2 * xy - xm * xy) / xd;
  double t1 = t2 - xd - xy;

  plot->update_time_axis(t1, t2, true);
}

// ----------------------------------------------------------------------------
bool ohlc_interactor::eventFilter(QObject* object, QEvent* event)
{
  if (object == nullptr || plot() == nullptr || object != plot()->canvas()) return false;

  switch (event->type())
  {
  // 2 finger trackpad movements appear as scroll events
  case QEvent::Wheel:
  {
    QWheelEvent* we = static_cast<QWheelEvent*>(event);
    m_data->initialPos = m_data->pos = we->position().toPoint();
    auto d = we->angleDelta();
    // sideways swipe
    if (std::abs(d.x()) >= std::abs(d.y())) { Q_EMIT panned(d.x() / 2, d.y() / 2); }
    // vertical swipe
    else { Q_EMIT zoomed(d.x() / 2, d.y() / 2); }
    break;
  }
  case QEvent::MouseButtonPress:
  {
    widgetMousePressEvent(static_cast<QMouseEvent*>(event));
    break;
  }
  case QEvent::MouseMove:
  {
    QMouseEvent* evr = static_cast<QMouseEvent*>(event);
    widgetMouseMoveEvent(evr);
    break;
  }
  case QEvent::MouseButtonRelease:
  {
    QMouseEvent* evr = static_cast<QMouseEvent*>(event);
    widgetMouseReleaseEvent(evr);
    break;
  }
  case QEvent::KeyPress:
  {
    widgetKeyPressEvent(static_cast<QKeyEvent*>(event));
    break;
  }
  case QEvent::KeyRelease:
  {
    widgetKeyReleaseEvent(static_cast<QKeyEvent*>(event));
    break;
  }

  case QEvent::NativeGesture:
  {
    qreal value = static_cast<QNativeGestureEvent*>(event)->value();

    if (value > 0)
    {
      inter_dbg<5>.debug(str<>("gesture"), static_cast<QNativeGestureEvent*>(event)->value());
    }
    else if (value < 0)
    {
      inter_dbg<5>.debug(str<>("gesture"), static_cast<QNativeGestureEvent*>(event)->value());
    }
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
void ohlc_interactor::setMouseButton(Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
  m_data->button = button;
  m_data->buttonModifiers = modifiers;
}

//! Get mouse button and modifiers used for panning
void ohlc_interactor::getMouseButton(
    Qt::MouseButton& button, Qt::KeyboardModifiers& modifiers) const
{
  button = m_data->button;
  modifiers = m_data->buttonModifiers;
}

/*!
   Change the abort key
   The defaults are Qt::Key_Escape and Qt::NoModifiers

   \param key Key (See Qt::Keycode)
   \param modifiers Keyboard modifiers
 */
void ohlc_interactor::setAbortKey(int key, Qt::KeyboardModifiers modifiers)
{
  m_data->abortKey = key;
  m_data->abortKeyModifiers = modifiers;
}

//! Get the abort key and modifiers
void ohlc_interactor::getAbortKey(int& key, Qt::KeyboardModifiers& modifiers) const
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
void ohlc_interactor::setEnabled(bool on)
{
  if (m_data->isEnabled != on)
  {
    m_data->isEnabled = on;

    if (this->plot() && this->plot()->canvas())
    {
      if (m_data->isEnabled) { this->plot()->canvas()->installEventFilter(this); }
      else { this->plot()->canvas()->removeEventFilter(this); }
    }
  }
}

/*!
   \return true when enabled, false otherwise
   \sa setEnabled, eventFilter()
 */
bool ohlc_interactor::isEnabled() const { return m_data->isEnabled; }

/*!
   Handle a mouse press event for the observed widget.

   \param mouseEvent Mouse event
   \sa eventFilter(), widgetMouseReleaseEvent(),
      widgetMouseMoveEvent(),
 */
void ohlc_interactor::widgetMousePressEvent(QMouseEvent* mouseEvent)
{
  m_data->initialPos = m_data->pos = mouseEvent->pos();
}

/*!
   Handle a mouse move event for the observed widget.

   \param mouseEvent Mouse event
   \sa eventFilter(), widgetMousePressEvent(), widgetMouseReleaseEvent()
 */
void ohlc_interactor::widgetMouseMoveEvent(QMouseEvent* mouseEvent)
{
  if (!parentWidget()->isVisible()) return;

  QPoint pos = mouseEvent->pos();
  if (pos != m_data->pos)
  {
    m_data->pos = pos;
    Q_EMIT moved(
        m_data->pos.x() - m_data->initialPos.x(), m_data->pos.y() - m_data->initialPos.y());
  }
}

/*!
   Handle a mouse release event for the observed widget.

   \param mouseEvent Mouse event
   \sa eventFilter(), widgetMousePressEvent(),
      widgetMouseMoveEvent(),
 */
void ohlc_interactor::widgetMouseReleaseEvent(QMouseEvent* mouseEvent)
{
  if (parentWidget()->isVisible())
  {
    QPoint pos = mouseEvent->pos();

    m_data->pos = pos;

    if (m_data->pos != m_data->initialPos) {}
  }
}

/*!
   Handle a key press event for the observed widget.

   \param keyEvent Key event
   \sa eventFilter(), widgetKeyReleaseEvent()
 */
void ohlc_interactor::widgetKeyPressEvent(QKeyEvent* keyEvent)
{
  if ((keyEvent->key() == m_data->abortKey) && (keyEvent->modifiers() == m_data->abortKeyModifiers))
  {
  }
  if (keyEvent->key() == Qt::Key_R)
  {
    timebased_chart_plot* plot = this->plot();
    if (plot == NULL) return;

    // get the X axis pixel/plot coordinate transform
    QwtScaleMap const mapx = plot->canvasMap(QwtAxis::XBottom);
    QwtScaleMap const mapy = plot->canvasMap(QwtAxis::YRight);

    // mouse pos in world coords
    double xm = mapx.invTransform(m_data->pos.x());
    double ym = mapy.invTransform(m_data->pos.y());

    Q_EMIT repair_pressed(QPointF(xm, ym));
  }
}

/*!
   Handle a key release event for the observed widget.

   \param keyEvent Key event
   \sa eventFilter(), widgetKeyReleaseEvent()
 */
void ohlc_interactor::widgetKeyReleaseEvent(QKeyEvent* keyEvent) { Q_UNUSED(keyEvent); }
