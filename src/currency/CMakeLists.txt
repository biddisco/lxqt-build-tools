set(grox_currency_SRCS
    currency_code.cpp
    currency_code.hpp
    currency.cpp
    currency.hpp
    currency_pair.cpp
    currency_pair.hpp
    json_data_types.cpp
    json_data_types.hpp
    ohlctv_sample.hpp
    trade_data.hpp
)

qt_add_library(grox_currency STATIC ${grox_currency_SRCS})
target_include_directories(grox_currency PUBLIC ${GROX_INCLUDE_ROOT})

target_link_libraries(
  grox_currency PUBLIC grox_util qwt Qt::Core nlohmann_json::nlohmann_json range-v3
)

if(GROX_WITH_IWYU)
  set_property(TARGET grox_currency PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
endif()

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.currency)
  pika_add_pseudo_dependencies(tests tests.currency)
  add_subdirectory(test)
endif()
