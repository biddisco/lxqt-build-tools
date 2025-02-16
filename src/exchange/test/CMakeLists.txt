option(GROX_AUTHENTICATION_TESTS "Enable tests requiring authentication" OFF)

set(TEST_SRCS)

if(GROX_AUTHENTICATION_TESTS)
  set(TEST_SRCS ${TEST_SRCS} qt-authenticate-bitstamp.cpp)
  set(qt-authenticate-bitstamp_PARAMETERS THREADS 2)
endif()

foreach(test ${TEST_SRCS})
  cmake_path(GET test STEM testname)
  add_executable(${testname} ${test})
  target_link_libraries(
    ${testname}
    PUBLIC grox_debug
           grox_util
           grox_currency
           grox_data
           grox_network
           grox_exchange
           GTest::gtest
           Threads::Threads
  )

  if(GROX_WITH_TESTING_SANITIZER)
    target_add_sanitizer_flags(${testname})
  endif()
  if(testname MATCHES "qt")
    # set_property(TARGET ${testname} PROPERTY AUTOMOC ON)
  endif()

  if(testname MATCHES "pika")
    set(${testname}_PARAMETERS "${${testname}_PARAMETERS}" THREADS 4)
    target_link_libraries(${testname} PUBLIC pika::pika)
  endif()

  grox_add_test_and_deps(
    "abstract_exchange" ${testname} EXECUTABLE ${testname} ${${testname}_PARAMETERS}
  )
  set_tests_properties("tests.abstract_exchange.${testname}" PROPERTIES TIMEOUT 30)
endforeach()
