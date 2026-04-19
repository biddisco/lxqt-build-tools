#include "widgets/indicators_model.hpp"

#include <QCommonStyle>
#include <QIcon>
#include <QPen>
#include <QStyle>
//
#include <QwtPlotCurve>
//
#include "indicators/indicator_params.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
indicators_model::indicators_model(QObject* parent)
  : QAbstractTableModel(parent)
{
}

int indicators_model::rowCount(QModelIndex const& /*parent*/) const { return indicators_.size(); }

int indicators_model::columnCount(QModelIndex const& /*parent*/) const { return 5; }

QVariant indicators_model::data(QModelIndex const& index, int role) const
{
  QVariant result;
  if (!index.isValid()) { return result; }

  auto it = std::next(indicators_.begin(), index.row());
  if (role == Qt::DisplayRole)
  {
    if (index.column() == 0) { return (to_qstring(it->ptr()->get_name())); }
    else if (index.column() == 1)
    {
      return to_qstring(indicators::param_string(it->ptr()->get_params()));
    }
    else if (index.column() == 2) { return (QString("")); }
  }
  else if (role == Qt::BackgroundRole && index.column() == 2)
  {
    QColor col = it->curves[0]->pen().color();
    return col;
  }
  else if (role == Qt::DecorationRole && index.column() == 3)
  {
    if (it->visibility_)
      return (QIcon(":/images/icons/pqEyeball.svg"));
    else
      return (QIcon(":/images/icons/pqEyeballClosed.svg"));
  }
  else if (role == Qt::DecorationRole && index.column() == 4)
  {
    return (QCommonStyle().standardIcon(QStyle::SP_TrashIcon));
  }

  return result;
}

// ----------------------------------------------------------------------------
void indicators_model::dataAdded()
{
  beginResetModel();
  QModelIndex topLeft = createIndex(0, 0);
  QModelIndex bottomRight = createIndex(indicators_.size(), 2);
  emit QAbstractTableModel::dataChanged(topLeft, bottomRight);
  endResetModel();
}