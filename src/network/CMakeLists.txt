find_package(Qt6 REQUIRED COMPONENTS Core WebSockets)

set(grox_network_HDRS evp-encrypt.hpp https-async.hpp qwebsocket_client.hpp
                      qwebsocket_session.hpp websocket-ssl.hpp)

set(grox_network_SRCS evp-encrypt.cpp https-async.cpp qwebsocket_client.cpp
                      qwebsocket_session.cpp websocket-ssl.cpp)

add_library(grox_network ${grox_network_SRCS} ${grox_network_HDRS})

target_include_directories(grox_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(grox_network PUBLIC GROX_HAVE_CURL_ENCODING
                                               GROX_HAVE_UUID_ENCODING)
target_link_libraries(grox_network PUBLIC grox_util fmt::fmt ${Boost_LIBRARIES}
                                          OpenSSL::SSL)
set_property(TARGET grox_network PROPERTY AUTOMOC ON)
target_link_libraries(grox_network PUBLIC Qt::Core Qt::WebSockets)

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.network)
  pika_add_pseudo_dependencies(tests tests.network)
  add_subdirectory(test)
endif()
