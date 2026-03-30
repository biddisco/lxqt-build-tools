# ------------------------------------------
# main tests
# ------------------------------------------
set(TEST_SRCS # display a chart with an indicator curve (sma) and a price curve (ohlc)
    # make sure all memory is freed cleanly
    chart_indicator.cpp
)

foreach(test ${TEST_SRCS})
  cmake_path(GET test STEM testname)
  add_executable(${testname} ${test})
  target_link_libraries(
    ${testname}
    PUBLIC grox_util
           grox_plot
           GTest::gtest
           OpenSSL::SSL
           Threads::Threads
           tests_plot_util
  )
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${testname} PRIVATE -Wno-deprecated-declarations)
  endif()
  if(GROX_WITH_TESTING_SANITIZER)
    target_add_sanitizer_flags(${testname})
  endif()

  if(testname MATCHES "pika")
    set(${testname}_PARAMETERS "${${testname}_PARAMETERS}" THREADS 4)
    target_link_libraries(${testname} PUBLIC pika::pika)
  endif()

  if(testname MATCHES "curl-bitstamp-authentication")
    target_link_libraries(${testname} PUBLIC curl)
  endif()

  grox_add_test_and_deps("plot" ${testname} EXECUTABLE ${testname} ${${testname}_PARAMETERS})
  set_tests_properties("tests.plot.${testname}" PROPERTIES TIMEOUT 30)

endforeach()

target_link_libraries(ripple-sign PUBLIC xrpl.libxrpl)
