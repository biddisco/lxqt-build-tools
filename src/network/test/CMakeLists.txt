option(GROX_AUTHENTICATION_TESTS "Enable tests requiring authentication" OFF)

# ------------------------------------------------------------------------------
# Boost
# ------------------------------------------------------------------------------
if(NOT Boost_FOUND)
  message(STATUS "Boost not found : network tests being skipped")
endif()

# ------------------------------------------
# utility library (program options)
# ------------------------------------------
add_library(tests_network_util INTERFACE)
target_link_libraries(tests_network_util INTERFACE Boost::headers Boost::program_options)

# ------------------------------------------
# main tests
# ------------------------------------------
# ~~~
set(TEST_SRCS
    # aes256 encrypt/decrypt
    aes256.cpp
    # ------------------------------------------
    # Ripple transaction signing demo : taken from rippled tests
    # ------------------------------------------
    ripple-sign.cpp
    # ------------------------------------------
    # obsolete boost asio tests : kept for reference
    # ------------------------------------------
    boost-https-request.cpp
    boost-websocket-bitstamp.cpp
    boost-websocket-xrpl.cpp
    # ------------------------------------------
    # qt networking websocket/http tests
    # ------------------------------------------
    # request xrp-gbp price history from bitstamp using https
    qt-https-bitstamp-price-history.cpp
    # request ohlc data from bitstamp using https
    qt-https-bitstamp-ohlc.cpp
    # websocket request to bitstamp for live trades (btc/usd)
    qt-websocket-bitstamp.cpp
    # http(s) request for xrpl data : ripple / local server
    qt-https-request-xrpl.cpp
    # websocket request for xrpl data : ripple / local server
    qt-websocket-xrpl.cpp
)

if(GROX_AUTHENTICATION_TESTS)
  set(TEST_SRCS
      ${TEST_SRCS}
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

set(local_rippled_server "oryx")

set(boost-https-request_PARAMETERS
    ARGS
    "--url"
    "www.bitstamp.net"
    "--port"
    "443"
    "--target"
    "/api/v2/ohlc/xrpusd/?step=60&limit=10"
)

set(boost-websocket-bitstamp_PARAMETERS
    ARGS
    "--url"
    "ws.bitstamp.net"
    "--port"
    "443"
    "--target"
    "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": \"live_trades_xrpusd\"}}"
)

# make a connection using standard ripple server
set(boost-websocket-xrpl_PARAMETERS ARGS "--url" "s1.ripple.com" "--port" "443")

# make a connection using standard local network server
set(boost-websocket-xrpl-local_PARAMETERS
    ARGS
    # ws: port is 6006 on our local server
    "--url" "${local_rippled_server}" "--port" "6006"
)

set(qt-https-request-xrpl_PARAMETERS ARGS "--url" "https://s1.ripple.com" "--port" "51234")
set(qt-https-request-xrpl-local_PARAMETERS
    ARGS
    # http port is 5005 on our local server
    "--url" "https://${local_rippled_server}" "--port" "5005"
)

set(qt-websocket-xrpl_PARAMETERS
    ARGS
    "--url"
    "s1.ripple.com"
    "--port"
    "443"
    "--timeout"
    "30"
)

set(qt-websocket-xrpl-local_PARAMETERS
    ARGS
    "--url"
    "${local_rippled_server}"
    "--port"
    "6006"
    "--timeout"
    "30"
)
# ~~~

set(curl-bitstamp-authentication_PARAMETERS ARGS "/api/v2/user_transactions/" "?limit=2")

set(boost-bitstamp-authentication_PARAMETERS ARGS "/api/v2/user_transactions/" "?&limit=2")

foreach(test ${TEST_SRCS})
  cmake_path(GET test STEM testname)
  qt_add_executable(${testname} ${test})
  target_link_libraries(
    ${testname}
    PRIVATE grox_util
            grox_network
            GTest::gtest
            OpenSSL::SSL
            Threads::Threads
            tests_network_util
  )
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${testname} PRIVATE -Wno-deprecated-declarations)
  endif()
  if(GROX_WITH_TESTING_SANITIZER)
    target_add_sanitizer_flags(${testname})
  endif()

  if(testname MATCHES "pika")
    set(${testname}_PARAMETERS "${${testname}_PARAMETERS}" THREADS 4)
    target_link_libraries(${testname} PRIVATE pika::pika)
  endif()

  if(testname MATCHES "curl-bitstamp-authentication")
    target_link_libraries(${testname} PRIVATE curl)
  endif()

  grox_add_test_and_deps("network" ${testname} EXECUTABLE ${testname} ${${testname}_PARAMETERS})
  set_tests_properties("tests.network.${testname}" PROPERTIES TIMEOUT 30)

  # for xrpl tests wee add the test twice, 1 once using ripple server, 2 once using local server
  # ("-local")
  if(testname MATCHES "-xrpl")
    grox_add_test_and_deps(
      "network"
      ${testname}-local
      PSEUDO_DEPS_NAME
      ${testname}
      EXECUTABLE
      ${testname}
      ${${testname}-local_PARAMETERS}
    )
  endif()

endforeach()

target_link_libraries(ripple-sign PRIVATE xrpl.libxrpl)
