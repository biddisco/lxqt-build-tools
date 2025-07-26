#pragma once

// Qt
#include <QWidget>
// Qwt
#include <QwtAxis>
#include <QwtAxisId>
#include <QwtScaleDiv>
// Grox
#include "data/ohlc_dataset_view.hpp"
#include "plot/ohlc_interactor.hpp"

class timebased_chart_plot;
class QCursor;
class QPixmap;

class QWT_EXPORT ohlc_interactor : public QObject
{
  Q_OBJECT

  public:
  ohlc_interactor(timebased_chart_plot* plot);
  virtual ~ohlc_interactor();

  QWidget* parentWidget();
  QWidget const* parentWidget() const;
  //
  timebased_chart_plot* plot();
  timebased_chart_plot const* plot() const;

  void setAxisEnabled(QwtAxisId axisId, bool on);
  bool isAxisEnabled(QwtAxisId) const;

  public:
  void setEnabled(bool);
  bool isEnabled() const;

  void setMouseButton(Qt::MouseButton, Qt::KeyboardModifiers = Qt::NoModifier);
  void getMouseButton(Qt::MouseButton& button, Qt::KeyboardModifiers&) const;

  void setAbortKey(int key, Qt::KeyboardModifiers = Qt::NoModifier);
  void getAbortKey(int& key, Qt::KeyboardModifiers&) const;

  void setCursor(QCursor const&);
  QCursor const cursor() const;

  void setOrientations(Qt::Orientations);
  Qt::Orientations orientations() const;

  bool isOrientationEnabled(Qt::Orientation) const;

  virtual bool eventFilter(QObject*, QEvent*) QWT_OVERRIDE;

  public Q_SLOTS:
  void panCanvas(int dx, int dy);
  void zoomCanvas(int dx, int dy);

  Q_SIGNALS:
  /*!
       Signal emitted, when panning is done

       \param dx Offset in horizontal direction
       \param dy Offset in vertical direction
     */
  void panned(int dx, int dy);
  void zoomed(int dx, int dy);

  /*!
       Signal emitted, while the widget moved, but panning
       is not finished.

       \param dx Offset in horizontal direction
       \param dy Offset in vertical direction
     */
  void moved(int dx, int dy);

  // mouse position in world coords at time of keypress
  void repair_pressed(QPointF pos);

  protected:
  virtual void widgetMouseWheelEvent(QWheelEvent*);
  virtual void widgetMousePressEvent(QMouseEvent*);
  virtual void widgetMouseReleaseEvent(QMouseEvent*);
  virtual void widgetMouseMoveEvent(QMouseEvent*);
  virtual void widgetKeyPressEvent(QKeyEvent*);
  virtual void widgetKeyReleaseEvent(QKeyEvent*);

  private:
  class PrivateData;
  PrivateData* m_data;
};
