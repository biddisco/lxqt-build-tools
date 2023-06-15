#pragma once

// STL
#include <vector>
// Qwt
#include <QwtInterval>
#include <QwtOHLCSample>
#include <QwtSeriesData>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_utils.hpp"
#include "plot/timebased_chart_data.hpp"

// ----------------------------------------------------------------------------
using ohlc_chart_data = timebased_chart_data<QwtOHLCSample>;
