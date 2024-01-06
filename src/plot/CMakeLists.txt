set(grox_plot_SRCS
    indicator_plot.cpp
    ohlc_date_scaledraw.cpp
    ohlc_price_plot.cpp
    OrderBookPlot.cpp
    timebased_data_curve.cpp
    ohlc_chart_curve.cpp
    ohlc_interactor.cpp
    OrderBookCurve.cpp
    timebased_chart_plot.cpp)

add_library(grox_plot ${grox_plot_SRCS})

set_property(TARGET grox_plot PROPERTY AUTOMOC ON)

target_include_directories(grox_plot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(grox_plot PUBLIC qwt grox_data)
