find_package(
  UUID
  REQUIRED
)
option(
  GROX_AUTHENTICATION_TESTS
  "Enable tests requiring authentication"
  OFF
)

# ~~~
set(TEST_SRCS
    # aes256 encrypt/decrypt
    aes256.cpp
    # Ripple transaction signing demo
    ripple-sign.cpp
    # obsolete boost asio tests
    boost-https-request.cpp
    boost-websocket-bitstamp.cpp
    boost-websocket-xrpl.cpp
    # qt networking tests
    qt-https-request-price-history.cpp
    qt-https-request-bitstamp.cpp
    qt-https-request-xrpl.cpp
    qt-websocket-bitstamp.cpp
)

if(GROX_AUTHENTICATION_TESTS)
  set(TEST_SRCS
      ${TEST_SRCS} #
      # token authentication using API key (bitstamp example)
      curl-bitstamp-authentication.cpp
      # token authentication using API key
      boost-bitstamp-authentication.cpp
      # login to bitstamp using API key
      qt-https-authentication.cpp
      # login to bitstamp using API key on pika threads
      qt-https-authentication-pika.cpp
  )
endif()
# ~~~

set(boost-websocket-bitstamp_PARAMETERS
    ARGS
    "ws.bitstamp.net"
    "443"
    "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": \"live_trades_xrpusd\"}}"
)

set(boost-https-request_PARAMETERS
    ARGS
    "www.bitstamp.net"
    "443"
    "/api/v2/ohlc/xrpusd/?step=60&limit=10"
)

set(curl-bitstamp-authentication_PARAMETERS
    ARGS
    "/api/v2/user_transactions/"
    "?limit=2"
)

set(boost-bitstamp-authentication_PARAMETERS
    ARGS
    "/api/v2/user_transactions/"
    "?&limit=2"
)

foreach(
  test
  ${TEST_SRCS}
)
  cmake_path(
    GET
    test
    STEM
    testname
  )
  add_executable(
    ${testname}
    ${test}
  )
  target_link_libraries(
    ${testname}
    PUBLIC grox_util
           grox_network
           GTest::gtest
           ${UUID_LIBRARIES}
           OpenSSL::SSL
           Threads::Threads
  )
  target_compile_definitions(
    ${testname}
    PRIVATE # needed for UUID generation in web requests
            GROX_HAVE_UUID_ENCODING
  )
  if(GROX_WITH_TESTING_SANITIZER)
    target_add_sanitizer_flags(${testname})
  endif()
  if(testname
     MATCHES
     "qt"
  )
    # set_property(TARGET ${testname} PROPERTY AUTOMOC ON)
  endif()

  if(testname
     MATCHES
     "pika"
  )
    set(${testname}_PARAMETERS
        "${${testname}_PARAMETERS}"
        THREADS
        4
    )
    target_link_libraries(
      ${testname}
      PUBLIC pika::pika
    )
  endif()

  if(testname
     MATCHES
     "curl-bitstamp-authentication"
  )
    target_link_libraries(
      ${testname}
      PUBLIC curl
    )
  endif()

  grox_add_test_and_deps(
    "network"
    ${testname}
    EXECUTABLE
    ${testname}
    ${${testname}_PARAMETERS}
  )
  set_tests_properties(
    "tests.network.${testname}"
    PROPERTIES TIMEOUT
               30
  )
endforeach()

target_link_libraries(
  ripple-sign
  PUBLIC xrpl.libxrpl
)
