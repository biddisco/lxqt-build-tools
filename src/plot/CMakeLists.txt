set(grox_plot_SRCS
    indicator_plot.cpp
    indicator_plot.hpp
    ohlc_chart_curve.cpp
    ohlc_chart_curve.hpp
    ohlc_date_scaledraw.cpp
    ohlc_date_scaledraw.hpp
    ohlc_interactor.cpp
    ohlc_interactor.hpp
    ohlc_picker.hpp
    ohlc_price_plot.cpp
    ohlc_price_plot.hpp
    OrderBookCurve.cpp
    OrderBookCurve.h
    OrderBookPlot.cpp
    OrderBookPlot.h
    timebased_chart_plot.cpp
    timebased_chart_plot.hpp
    timebased_data_curve.cpp
    timebased_data_curve.hpp
)

add_library(grox_plot ${grox_plot_SRCS})

set_property(TARGET grox_plot PROPERTY AUTOMOC ON)

target_include_directories(grox_plot PUBLIC ${GROX_INCLUDE_ROOT})

target_link_libraries(
  grox_plot
  PUBLIC qwt
         grox_data
         grox_debug
         grox_util
         grox_currency
         grox_exchange
         fmt::fmt
         range-v3
         Qt::Core
)
