find_package(cppzmq REQUIRED)

set(GROX_BACKTESTER_SRC main.cpp)

add_executable(backtester ${GROX_BACKTESTER_SRC})

set_property(TARGET backtester PROPERTY CXX_STANDARD ${GROX_CXX_STANDARD})

target_include_directories(
  backtester PUBLIC ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
  backtester
  PUBLIC grox_currency
         Boost::program_options
         grox_data
         grox_debug
         grox_indicators
         grox_io
         grox_util
         Qt::Core
         nlohmann_json::nlohmann_json
         cppzmq
)
# Test client for ZeroMQ mode
add_executable(backtester_test_client test_client.cpp)
set_property(TARGET backtester_test_client PROPERTY CXX_STANDARD ${GROX_CXX_STANDARD})
target_link_libraries(backtester_test_client PUBLIC nlohmann_json::nlohmann_json cppzmq)
