#pragma once

#include "data/ohlctv_sample.hpp"
#include "data/timebased_chart_data.hpp"

// ----------------------------------------------------------------------------
using ohlc_chart_data = timebased_chart_data<ohlctv_sample>;
