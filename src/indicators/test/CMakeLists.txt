set(TEST_SRCS averages.cpp)

foreach(test ${TEST_SRCS})
  cmake_path(GET test STEM testname)
  add_executable(${testname} ${test})
  target_link_libraries(${testname} PUBLIC grox_indicators GTest::gtest)
  set(${testname}_PARAMETERS "${${testname}_PARAMETERS}" THREADS 0)
  grox_add_test_and_deps("indicators" ${testname} EXECUTABLE ${testname} ${${testname}_PARAMETERS})
endforeach()
