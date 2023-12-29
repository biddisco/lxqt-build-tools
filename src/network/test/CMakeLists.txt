find_package(UUID REQUIRED)
option(GROX_AUTHRNTICATION_TESTS "Enable tests requireing authentication" OFF)

set(TEST_SRCS
    websocket-bitstamp.cpp # * websocket test bitstamp
    websocket-xrpl.cpp # * websocket test xrpl
    https.cpp # https access
    aes256.cpp # * aes256 encrypt/decrypt
    ripple-sign.cpp # * Ripple transaction signing demo
    belle-client.cpp # * bitstamp connection test using belle
    belle-json-request.cpp # * xrpl json https request using belle
    websocket_qt_bitstamp.cpp # websocket bitstamp SSL using Qt networking
)

if(GROX_AUTHRNTICATION_TESTS)
  set(TEST_SRCS login.cpp # token authentication
                bitstamp.cpp # bitstamp official example code
  )
endif()

set(websocket-bitstamp_PARAMETERS
    ARGS
    "ws.bitstamp.net"
    "443"
    "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": \"live_trades_xrpusd\"}}"
)

set(https_PARAMETERS ARGS "www.bitstamp.net" "443"
                     "/api/v2/ohlc/xrpusd/?step=60&limit=10")

foreach(test ${TEST_SRCS})
  cmake_path(GET test STEM testname)
  add_executable(${testname} ${test})
  target_link_libraries(
    ${testname}
    PUBLIC grox_util
           grox_network
           nlohmann_json
           belle
           ${UUID_LIBRARIES}
           OpenSSL::SSL
           Threads::Threads)
  target_compile_definitions(
    ${testname} PRIVATE # needed for UUID generation in web requests
                        GROX_HAVE_UUID_ENCODING)
  if(GROX_WITH_TESTING_SANITIZER)
    target_add_sanitizer_flags(${testname})
  endif()
  if(testname MATCHES "belle")
    target_link_libraries(${testname} PUBLIC belle)
    target_compile_definitions(
      ${testname}
      PRIVATE # disable support for belle websocket server
              OB_BELLE_CONFIG_SERVER_OFF
              # set certificate path for belle original test
              BELLE_CERT_PATH="${belle_SOURCE_DIR}/example/cert")
  endif()
  set(${testname}_PARAMETERS "${${testname}_PARAMETERS}" THREADS 4)
  grox_add_test_and_deps("network" ${testname} EXECUTABLE ${testname}
                         ${${testname}_PARAMETERS})
endforeach()

target_link_libraries(ripple-sign PUBLIC Ripple::xrpl_core)
if(GROX_AUTHRNTICATION_TESTS)
  target_link_libraries(bitstamp PUBLIC curl)
endif()
