set(TEST_SRCS averages.cpp trade.cpp volatility.cpp python_indicators.cpp ownership.cpp)

foreach(test ${TEST_SRCS})
  cmake_path(GET test STEM testname)
  qt_add_executable(${testname} ${test})
  target_link_libraries(${testname} PRIVATE grox_indicators GTest::gtest)
  set(${testname}_PARAMETERS "${${testname}_PARAMETERS}" THREADS 0)
  grox_add_test_and_deps("indicators" ${testname} EXECUTABLE ${testname} ${${testname}_PARAMETERS})
endforeach()
