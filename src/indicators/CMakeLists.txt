find_package(Qt6 REQUIRED COMPONENTS Core)

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
    # Python support (headers included, implementation below)
    python_bindings.hpp
    python_plugin.hpp
    python_plugin.cpp
)

add_library(grox_indicators ${grox_indicators_SRCS})
target_include_directories(grox_indicators PUBLIC ${GROX_INCLUDE_ROOT})

find_package(spdlog QUIET)

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

if(spdlog_FOUND)
  target_link_libraries(grox_indicators PUBLIC spdlog::spdlog)
  target_compile_definitions(grox_indicators PUBLIC GROX_HAS_SPDLOG=1)
  message(STATUS "Indicators: SPDLOG enabled")
else()
  target_compile_definitions(grox_indicators PUBLIC GROX_HAS_SPDLOG=0)
  message(STATUS "Indicators: SPDLOG not found, SPDLOG logging disabled")
endif()

# ==============================================================================
# Macro for creating indicator plugins
# ==============================================================================
macro(add_indicator_plugin plugin_name plugin_source plugin_headers)
  add_library(${plugin_name} SHARED ${plugin_source} ${plugin_headers})
  target_include_directories(${plugin_name} PRIVATE ${GROX_INCLUDE_ROOT})
  target_link_libraries(${plugin_name} PRIVATE grox_indicators qwt grox_data Qt::Core fmt::fmt)

  set_target_properties(
    ${plugin_name}
    PROPERTIES PREFIX ""
               OUTPUT_NAME "${ARGV3}"
               SUFFIX ".so"
               VERSION "1.0.0"
               SOVERSION "1"
               CXX_VISIBILITY_PRESET hidden
               LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/grox/plugins"
  )

  install(TARGETS ${plugin_name} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/grox/plugins)
endmacro()

# ==============================================================================
# Plugin: Moving Averages
# ==============================================================================
add_indicator_plugin(
  grox_plugin_moving_averages
  plugins/moving_averages_plugin.cpp
  "moving_average.hpp;moving_average_exponential.hpp;moving_average_exponential_volume_weighted.hpp;moving_average_hull.hpp;moving_average_volume_weighted.hpp"
  moving_averages
)

# ==============================================================================
# Plugin: Volatility Indicators
# ==============================================================================
add_indicator_plugin(
  grox_plugin_volatility plugins/volatility_plugin.cpp
  "volatility_bollinger_bands.hpp;volatility_garman_klass.hpp;volatility_rogers_satchell.hpp"
  volatility
)

# ==============================================================================
# Plugin: Technical Indicators
# ==============================================================================
add_indicator_plugin(
  grox_plugin_technical_indicators
  plugins/technical_indicators_plugin.cpp
  "relative_strength_indicator.hpp;stochastic_oscillator.hpp;stochastic_relative_strength_indicator.hpp"
  technical_indicators
)

# ==============================================================================
# Plugin: Trading Algorithms
# ==============================================================================
add_indicator_plugin(
  grox_plugin_trading
  plugins/trading_plugin.cpp
  "trade_arbitrage_2_way.hpp;trade_currency_exchange.hpp;trade_market_maker.hpp;trade_sell_sliding_stop.hpp;trade_rebalance_funds.hpp"
  trading
)

# ==============================================================================
# Python Indicator Support (optional)
# ==============================================================================
option(GROX_WITH_PYTHON_INDICATORS "Enable Python-based dynamic indicators" ON)

if(GROX_WITH_PYTHON_INDICATORS)
  # Try to find Python
  find_package(Python3 COMPONENTS Interpreter Development QUIET)

  if(Python3_FOUND)
    target_include_directories(grox_indicators PUBLIC ${Python3_INCLUDE_DIRS})
    target_link_libraries(grox_indicators PUBLIC ${Python3_LIBRARIES})

    # Add Python path to indicators library for embedded interpreter
    target_compile_definitions(
      grox_indicators PRIVATE GROX_PYTHON_ENABLED=1 GROX_PYTHON_VERSION="${Python3_VERSION}"
    )

    message(STATUS "Python indicators enabled: Python ${Python3_VERSION}")
  else()
    message(WARNING "Python development headers not found, Python indicators disabled")
    set(GROX_WITH_PYTHON_INDICATORS OFF)
  endif()
else()
  # Stub implementation without Python
  target_compile_definitions(grox_indicators PRIVATE GROX_PYTHON_ENABLED=0)
endif()

# ==============================================================================
# Tests
# ==============================================================================
if(BUILD_TESTING)
  pika_add_pseudo_target(tests.indicators)
  pika_add_pseudo_dependencies(tests tests.indicators)
  add_subdirectory(test)
endif()
