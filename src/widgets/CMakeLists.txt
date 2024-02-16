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
    indicator_dialog.cpp
    indicator_dialog.hpp
    password_dialog.cpp
    password_dialog.hpp
    price_chart_widget.cpp
    price_chart_widget.hpp
    trade_widget.cpp
    trade_widget.hpp
    wallet_widget.cpp
    wallet_widget.hpp
    xrp_functions.cpp
    xrp_functions.hpp
)

# qt_wrap_ui(grox_widgets_ui_SRC connection_widget.ui currency_widget.ui indicator_dialog.ui
# password_dialog.ui price_chart_widget.ui trade_widget.ui wallet_widget.ui xrp_functions.ui)

set(grox_widgets_SRCS
    ${grox_widgets_SRCS}
    connection_widget.ui
    currency_widget.ui
    indicator_dialog.ui
    password_dialog.ui
    price_chart_widget.ui
    trade_widget.ui
    wallet_widget.ui
    xrp_functions.ui
)

add_library(
  grox_widgets
  ${grox_widgets_SRCS}
  ${grox_widgets_ui_SRC}
)

set_property(
  TARGET grox_widgets
  PROPERTY AUTOMOC
           ON
)
set_property(
  TARGET grox_widgets
  PROPERTY AUTOUIC
           ON
)

target_include_directories(grox_widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(
  grox_widgets
  PUBLIC grox_currency
         grox_data
         grox_debug
         grox_exchange
         grox_plot
         grox_util
         fmt::fmt
         nlohmann_json
         range-v3
         Qt::Core
         Qt::Widgets
)
