find_package(cppzmq REQUIRED)

set(GROX_BACKTESTER_SRC main.cpp)

qt_add_executable(backtester ${GROX_BACKTESTER_SRC})

target_include_directories(
  backtester PRIVATE ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
                     ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
  backtester
  PRIVATE grox_currency
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
qt_add_executable(backtester_test_client test_client.cpp)
target_link_libraries(backtester_test_client PRIVATE nlohmann_json::nlohmann_json cppzmq)
