set(grox_widgets_control_SRCS control_builder.hpp control_builder.cpp control_factory.hpp)

set(grox_widgets_control_SRCS ${grox_widgets_control_SRCS}
                              # trade_algorithm_arbitrage.ui
)

add_library(
  grox_widgets_control ${grox_widgets_control_SRCS} ${grox_widgets_control_ui_SRC}
                       ${grox_widgets_control_QRCS} ${grox_widgets_control_UI}
)

set_property(TARGET grox_widgets_control PROPERTY AUTOMOC ON)
set_property(TARGET grox_widgets_control PROPERTY AUTOUIC ON)
set_property(TARGET grox_widgets_control PROPERTY AUTORCC ON)

target_include_directories(grox_widgets_control PUBLIC ${GROX_INCLUDE_ROOT})

target_link_libraries(
  grox_widgets_control
  PUBLIC grox_currency
         grox_data
         grox_debug
         grox_exchange
         grox_indicators
         grox_plot
         grox_util
         qtadvanceddocking-qt${QT_VERSION_MAJOR}
         fmt::fmt
         nlohmann_json::nlohmann_json
         range-v3
         Qt::Core
         Qt::Widgets
)

if(BUILD_TESTING AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/test))
  pika_add_pseudo_target(tests.widgets_control)
  pika_add_pseudo_dependencies(tests tests.widgets_control)
  add_subdirectory(test)
endif()
