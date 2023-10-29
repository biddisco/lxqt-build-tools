#pragma once

// STL
#include <vector>
// Qwt
#include <QwtInterval>
#include <QwtSeriesData>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_utils.hpp"
#include "data/ohlctv_sample.hpp"
#include "plot/timebased_chart_data.hpp"

// ----------------------------------------------------------------------------
using ohlc_chart_data = timebased_chart_data<ohlctv_sample>;
