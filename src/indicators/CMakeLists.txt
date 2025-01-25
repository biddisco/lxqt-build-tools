find_package(Qt6 REQUIRED COMPONENTS Core)

set(grox_indicators_SRCS
    # basic definitions
    algorithm_base.hpp
    indicator_base.hpp
    indicator_params.hpp
    indicator_ptr.hpp
    indicator_registry.hpp
    indicator_registry.cpp
    indicator_types.hpp
    # moving averages
    moving_average_exponential.hpp
    moving_average_exponential_volume_weighted.hpp
    moving_average.hpp
    moving_average_volume_weighted.hpp
    # others
    relative_strength_indicator.hpp
    stochastic_oscillator.hpp
    stochastic_relative_strength_indicator.hpp
    # volatility estimators
    volatility_bollinger_bands.hpp
    volatility_garman_klass.hpp
    volatility_rogers_satchell.hpp
    # trading
    trade_arbitrage_2_way.hpp
    trade_currency_exchange.hpp
    trade_market_maker.hpp
    trade_sell_sliding_stop.hpp
)

add_library(grox_indicators ${grox_indicators_SRCS})
target_include_directories(grox_indicators PUBLIC ${GROX_INCLUDE_ROOT})

target_link_libraries(
  grox_indicators
  PUBLIC qwt
         grox_data
         grox_util
         grox_io
         Qt::Core
         fmt::fmt
         ${Boost_LIBRARIES}
)

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.indicators)
  pika_add_pseudo_dependencies(tests tests.indicators)
  add_subdirectory(test)
endif()
