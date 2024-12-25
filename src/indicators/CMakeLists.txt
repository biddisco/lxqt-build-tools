set(grox_indicators_SRCS
    # basic definitions
    algorithm_base.hpp
    indicator_base.hpp
    indicator_ptr.hpp
    indicator_params.hpp
    indicator_types.hpp
    # volatility estimators
    volatility_bollinger_bands.hpp
    volatility_garman_klass.hpp
    volatility_rogers_satchell.hpp
    # moving averages
    moving_average_exponential.hpp
    moving_average_exponential_volume_weighted.hpp
    moving_average.hpp
    moving_average_volume_weighted.hpp
    # others
    relative_strength_indicator.hpp
    stochastic_oscillator.hpp
    stochastic_relative_strength_indicator.hpp
    # trading
    sliding_stop.hpp
)

add_library(
  grox_indicators INTERFACE
  # ${grox_indicators_SRCS}
)

target_include_directories(grox_indicators INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(grox_indicators INTERFACE qwt grox_data grox_io fmt::fmt ${Boost_LIBRARIES})

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.indicators)
  pika_add_pseudo_dependencies(tests tests.indicators)
  add_subdirectory(test)
endif()
