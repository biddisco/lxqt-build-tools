find_package(
  Qt6
  REQUIRED
  COMPONENTS Core
             Network
)

set(grox_exchange_SRCS
    account.hpp
    bitstamp.hpp
    bitstamp.cpp
    exchange.hpp
    exchange.cpp
    order_book.hpp
    order_book.cpp
    xrpl.hpp
    xrpl.cpp
    xrpl_network.hpp
    xrpl_network.cpp
)

add_library(grox_exchange ${grox_exchange_SRCS})
set_property(
  TARGET grox_exchange
  PROPERTY AUTOMOC
           ON
)
target_include_directories(grox_exchange PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(
  grox_exchange
  PUBLIC grox_debug
         grox_util
         grox_data
         grox_network
         nlohmann_json
         Ripple::xrpl_core
         range-v3
         qwt
         Qt::Core
         Qt::Network
)

if(GROX_WITH_IWYU)
  set_property(
    TARGET grox_exchange
    PROPERTY CXX_INCLUDE_WHAT_YOU_USE
             ${iwyu_path}
  )
endif()
