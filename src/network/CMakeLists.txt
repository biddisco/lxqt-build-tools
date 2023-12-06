set(grox_network_SRCS evp-encrypt.cpp https-async.cpp websocket-ssl.cpp)

add_library(grox_network ${grox_network_SRCS})
target_include_directories(grox_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(grox_network PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL)

target_compile_definitions(grox_network PUBLIC GROX_HAVE_CURL_ENCODING GROX_HAVE_UUID_ENCODING)

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.network)
  pika_add_pseudo_dependencies(tests tests.network)
  add_subdirectory(test)
endif()
