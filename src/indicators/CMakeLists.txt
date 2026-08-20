# Core indicator library (static) Contains only base classes and plugin infrastructure
set(grox_indicators_SRCS
    # basic definitions
    algorithm_base.hpp
    indicator_base.hpp
    indicator_params.hpp
    indicator_ptr.hpp
    indicator_partitioner.hpp
    indicator_registry.hpp
    indicator_registry.cpp
    indicator_types.hpp
    # Plugin support
    plugin_api.hpp
    plugin_loader.hpp
    plugin_loader.cpp
)

if(GROX_WITH_PYTHON_INDICATORS)
  set(grox_indicators_SRCS
      ${grox_indicators_SRCS}
      # Python support (headers included, implementation below)
      python_bindings.hpp python_plugin.hpp python_plugin.cpp
  )
endif()

qt_add_library(grox_indicators STATIC ${grox_indicators_SRCS})
target_include_directories(grox_indicators PUBLIC ${GROX_INCLUDE_ROOT})

target_link_libraries(
  grox_indicators
  PUBLIC qwt
         grox_data
         grox_util
         grox_io
         Qt::Core
         fmt::fmt
         ${CMAKE_DL_LIBS} # For dlopen/dlsym
)

# ==============================================================================
# Macro for creating indicator plugins
# ==============================================================================
function(add_indicator_plugin)
  set(options)
  set(oneValueArgs NAME OUTPUT_NAME)
  set(multiValueArgs SOURCES HEADERS)
  cmake_parse_arguments(PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT PLUGIN_NAME)
    message(FATAL_ERROR "add_indicator_plugin: NAME is required")
  endif()
  if(NOT PLUGIN_SOURCES)
    message(FATAL_ERROR "add_indicator_plugin: SOURCES is required")
  endif()

  add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})
  target_include_directories(${PLUGIN_NAME} PRIVATE ${GROX_INCLUDE_ROOT})
  target_link_libraries(${PLUGIN_NAME} PRIVATE grox_indicators qwt grox_data Qt::Core fmt::fmt)

  set_target_properties(
    ${PLUGIN_NAME}
    PROPERTIES PREFIX ""
               OUTPUT_NAME "${PLUGIN_OUTPUT_NAME}"
               SUFFIX ".so"
               VERSION "1.0.0"
               SOVERSION "1"
               CXX_VISIBILITY_PRESET hidden
               LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/grox/plugins"
  )

  install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/grox/plugins)
endfunction()

# ==============================================================================
# Plugin: Moving Averages
# ==============================================================================
add_indicator_plugin(
  NAME
  grox_plugin_moving_averages
  SOURCES
  plugins/moving_averages_plugin.cpp
  HEADERS
  moving_average.hpp
  moving_average_convergence_divergence.hpp
  moving_average_cross.hpp
  moving_average_exponential.hpp
  moving_average_exponential_volume_weighted.hpp
  moving_average_hull.hpp
  moving_average_volume_weighted.hpp
  OUTPUT_NAME
  moving_averages
)

# ==============================================================================
# Plugin: Volatility Indicators
# ==============================================================================
add_indicator_plugin(
  NAME
  grox_plugin_volatility
  SOURCES
  plugins/volatility_plugin.cpp
  HEADERS
  volatility_bollinger_bands.hpp
  volatility_garman_klass.hpp
  volatility_rogers_satchell.hpp
  OUTPUT_NAME
  volatility
)

# ==============================================================================
# Plugin: Technical Indicators
# ==============================================================================
add_indicator_plugin(
  NAME
  grox_plugin_technical_indicators
  SOURCES
  plugins/technical_indicators_plugin.cpp
  HEADERS
  average_directional_index.hpp
  average_true_range.hpp
  commodity_channel_index.hpp
  ichimoku_cloud.hpp
  on_balance_volume.hpp
  parabolic_sar.hpp
  rate_of_change.hpp
  relative_strength_indicator.hpp
  stochastic_oscillator.hpp
  stochastic_relative_strength_indicator.hpp
  volume_weighted_average_price.hpp
  williams_percent_r.hpp
  OUTPUT_NAME
  technical_indicators
)

# ==============================================================================
# Plugin: Trading Algorithms
# ==============================================================================
add_indicator_plugin(
  NAME
  grox_plugin_trading
  SOURCES
  plugins/trading_plugin.cpp
  HEADERS
  trade_arbitrage_2_way.hpp
  trade_currency_exchange.hpp
  trade_market_maker.hpp
  trade_sell_sliding_stop.hpp
  trade_rebalance_funds.hpp
  OUTPUT_NAME
  trading
)

# ==============================================================================
# Python Indicator Support (optional)
# ==============================================================================
if(GROX_WITH_PYTHON_INDICATORS)
  # Try to find Python
  find_package(Python3 COMPONENTS Interpreter Development QUIET)

  if(Python3_FOUND)
    target_include_directories(grox_indicators PUBLIC ${Python3_INCLUDE_DIRS})
    target_link_libraries(grox_indicators PUBLIC ${Python3_LIBRARIES})
    set(GROX_PYTHON_VERSION "${Python3_VERSION}")

    execute_process(
      COMMAND ${Python3_EXECUTABLE} -c "import sys; print(sys.base_prefix)"
      OUTPUT_VARIABLE GROX_PYTHON_BASE_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
    )

    add_config_define_namespace(
      NAMESPACE PYTHON DEFINE GROX_PYTHON_EXECUTABLE VALUE "\"${Python3_EXECUTABLE}\""
    )
    add_config_define_namespace(
      NAMESPACE PYTHON DEFINE GROX_PYTHON_VERSION VALUE "\"${Python3_VERSION}\""
    )
    if(GROX_PYTHON_BASE_PREFIX)
      add_config_define_namespace(
        NAMESPACE PYTHON DEFINE GROX_PYTHON_BASE_PREFIX VALUE "\"${GROX_PYTHON_BASE_PREFIX}\""
      )
    endif()

    add_config_define_namespace(NAMESPACE PYTHON DEFINE GROX_PYTHON_INDICATORS VALUE 1)
    message(STATUS "Python indicators enabled: Python ${Python3_VERSION} (${Python3_EXECUTABLE})")
  else()
    message(WARNING "Python development headers not found, Python indicators disabled")
    set(GROX_WITH_PYTHON_INDICATORS OFF CACHE BOOL "Enable Python-based dynamic indicators" FORCE)
  endif()
endif()

# ==============================================================================
# Tests
# ==============================================================================
if(BUILD_TESTING)
  pika_add_pseudo_target(tests.indicators)
  pika_add_pseudo_dependencies(tests tests.indicators)
  add_subdirectory(test)
endif()
