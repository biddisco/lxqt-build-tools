find_package(Qt6 REQUIRED COMPONENTS Core WebSockets)

find_package(UUID REQUIRED)

set(grox_network_HDRS evp-encrypt.hpp qwebsocket_client.hpp qhttp-request-client.hpp
                      qwebsocket_session.hpp
)

set(grox_network_SRCS evp-encrypt.cpp qwebsocket_client.cpp qhttp-request-client.cpp
                      qwebsocket_session.cpp
)

add_library(grox_network ${grox_network_SRCS} ${grox_network_HDRS})

target_include_directories(grox_network PUBLIC ${GROX_INCLUDE_ROOT})
target_compile_definitions(grox_network PUBLIC GROX_HAVE_CURL_ENCODING GROX_HAVE_UUID_ENCODING)
target_link_libraries(
  grox_network PUBLIC grox_util fmt::fmt nlohmann_json::nlohmann_json ${Boost_LIBRARIES}
                      OpenSSL::SSL
)
set_property(TARGET grox_network PROPERTY AUTOMOC ON)
target_link_libraries(grox_network PUBLIC grox_debug Qt::Core Qt::WebSockets ${UUID_LIBRARY})

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.network)
  pika_add_pseudo_dependencies(tests tests.network)
  add_subdirectory(test)
endif()
