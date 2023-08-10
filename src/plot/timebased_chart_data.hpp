#pragma once

// STL
#include <vector>
// Qwt
#include <QwtInterval>
#include <QwtOHLCSample>
#include <QwtSeriesData>
//
#include "data/ohlc_utils.hpp"

// ----------------------------------------------------------------------------
template <typename DataType>
class timebased_chart_data : public QwtArraySeriesData<DataType>
{
  protected:
  double resolution_;

  using QwtArraySeriesData<DataType>::m_samples;
  using QwtArraySeriesData<DataType>::cachedBoundingRect;

  public:
  timebased_chart_data(double resolution)
    : QwtArraySeriesData<DataType>()
    , resolution_(resolution)
  {
  }

  ~timebased_chart_data() {}

  minmax_data<DataType> minmax_limits(size_t from, size_t to) const
  {
    auto const& init = m_samples[from];
    minmax_data<DataType> result(init);
    for (size_t i = from; i <= to; ++i)
    {
      result.update(m_samples[i]);
    }
    return result;
  }

  inline void append(DataType const& data)
  {
    m_samples += data;
  }

  void clear()
  {
    m_samples.clear();
    m_samples.squeeze();
    cachedBoundingRect = QRectF(0.0, 0.0, -1.0, -1.0);
  }

  inline double get_resolution() const
  {
    return resolution_;
  }

  // return the index of the sample at time t
  inline int64_t sample_index(double time) const
  {
    int64_t i = static_cast<int64_t>((time - get_time(m_samples[0])) / resolution_);
    return std::max(int64_t(0), i);
  }

  // return the time stamp for the sample at index i
  inline double sample_time(int64_t i) const
  {
    double t = (i * resolution_) + get_time(m_samples[0]);
    return t;
  }

  inline QVector<DataType> const& data() const
  {
    return m_samples;
  }
  inline QVector<DataType>& data()
  {
    return m_samples;
  }
};
