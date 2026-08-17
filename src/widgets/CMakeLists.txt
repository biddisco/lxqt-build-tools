set(grox_widgets_SRCS
    check_trades_dialog.cpp
    check_trades_dialog.hpp
    collapsible_groupbox.cpp
    collapsible_groupbox.hpp
    connection_widget.cpp
    connection_widget.hpp
    currency_widget.cpp
    currency_widget.hpp
    digital_clock.cpp
    digital_clock.hpp
    indicators_model.cpp
    indicators_model.hpp
    transactions_model.cpp
    transactions_model.hpp
    password_dialog.cpp
    password_dialog.hpp
    price_chart_widget.cpp
    price_chart_widget.hpp
    trade_widget.cpp
    trade_widget.hpp
    trade_algorithm_widget.cpp
    trade_algorithm_widget.hpp
    wallet_widget.cpp
    wallet_widget.hpp
    xrp_functions.cpp
    xrp_functions.hpp
)

set(grox_widgets_SRCS
    ${grox_widgets_SRCS}
    trade_algorithm_arbitrage.ui
    trade_algorithm_currency_exchange.ui
    trade_algorithm_market_maker.ui
    connection_widget.ui
    currency_widget.ui
    password_dialog.ui
    price_chart_widget.ui
    trade_widget.ui
    wallet_widget.ui
    xrp_functions.ui
)

set(grox_widgets_QRCS images.qrc)

add_library(
  grox_widgets ${grox_widgets_SRCS} ${grox_widgets_ui_SRC} ${grox_widgets_QRCS} ${grox_widgets_UI}
)

set_property(TARGET grox_widgets PROPERTY AUTOMOC ON)
set_property(TARGET grox_widgets PROPERTY AUTOUIC ON)
set_property(TARGET grox_widgets PROPERTY AUTORCC ON)

target_include_directories(grox_widgets PUBLIC ${GROX_INCLUDE_ROOT})

target_link_libraries(
  grox_widgets
  PUBLIC grox_currency
         grox_data
         grox_debug
         grox_exchange
         grox_indicators
         grox_io
         grox_plot
         grox_util
         grox_widgets_control
         qtadvanceddocking-qt${QT_VERSION_MAJOR}
         fmt::fmt
         nlohmann_json::nlohmann_json
         range-v3
         Qt::Core
         Qt::Widgets
)
