find_package(Qt6 REQUIRED COMPONENTS Core Network)

set(grox_exchange_SRCS
    account.hpp
    bitstamp.hpp
    bitstamp.cpp
    abstract_exchange.hpp
    abstract_exchange.cpp
    order_book_bitstamp.hpp
    order_book_bitstamp.cpp
    order_book_xrpl.hpp
    order_book_xrpl.cpp
    xrpl.hpp
    xrpl.cpp
    xrpl_network.hpp
    xrpl_network.cpp
)

add_library(grox_exchange ${grox_exchange_SRCS} ticker_data.hpp)
target_include_directories(grox_exchange PUBLIC ${GROX_INCLUDE_ROOT})

set_property(TARGET grox_exchange PROPERTY AUTOMOC ON)

target_link_libraries(
  grox_exchange
  PUBLIC grox_debug
         grox_util
         grox_currency
         grox_data
         grox_io
         grox_network
         grox_util
         nlohmann_json::nlohmann_json
         xrpl.libxrpl
         range-v3
         qwt
         Qt::Core
         Qt::Network
)

if(GROX_WITH_IWYU)
  set_property(TARGET grox_exchange PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
endif()

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.abstract_exchange)
  pika_add_pseudo_dependencies(tests tests.abstract_exchange)
  add_subdirectory(test)
endif()
