set(GROXCLI_SRC main.cpp)

add_executable(grox-cli ${GROXCLI_SRC} ${GROXCLI_QRC_SRC})

# c++ properties
set_property(TARGET grox-cli PROPERTY CXX_STANDARD ${GROX_CXX_STANDARD})

target_include_directories(
  grox-cli PUBLIC ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
                  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  set(STD_FILESYSTEM_LIB "stdc++fs")
endif()

if(GROXCLI_WITH_SANITIZER)
  target_add_sanitizer_flags(grox-cli)
endif()

target_compile_definitions(
  grox-cli PRIVATE # needed for UUID generation in web requests
                   GROXCLI_HAVE_UUID_ENCODING
)

target_link_libraries(
  grox-cli
  PUBLIC grox_currency
         grox_data
         grox_debug
         grox_exchange
         grox_indicators
         grox_io
         grox_network
         grox_plot
         grox_util
         Qt::Core
         ${Boost_LIBRARIES}
         nlohmann_json::nlohmann_json
         STDEXEC::stdexec
         fmt::fmt
         range-v3
         xrpl.libxrpl
         uuid
         Threads::Threads
         pika::pika
)
