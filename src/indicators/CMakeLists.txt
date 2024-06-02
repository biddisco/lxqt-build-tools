set(grox_indicators_SRCS
    # basic definitions
    indicator_definitions.hpp
    indicator_types.hpp
    # volatility estimators
    bollinger_bands.hpp
    garman_klass_volatility.hpp
    rogers_satchell_volatility.hpp
    # moving averages
    moving_average_exponential.hpp
    moving_average_exponential_volume_weighted.hpp
    moving_average.hpp
    moving_average_volume_weighted.hpp
    # others
    relative_strength_indicator.hpp
    stochastic_oscillator.hpp
    stochastic_relative_strength_indicator.hpp
)

add_library(
  grox_indicators
  INTERFACE
)

target_include_directories(
  grox_indicators
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(
  grox_indicators
  INTERFACE qwt
            grox_data
            grox_io
            fmt::fmt
            ${Boost_LIBRARIES}
)

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.indicators)
  pika_add_pseudo_dependencies(
    tests
    tests.indicators
  )
  add_subdirectory(test)
endif()
