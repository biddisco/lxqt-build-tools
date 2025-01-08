set(TEST_SRCS datetime.cpp)

foreach(test ${TEST_SRCS})
  cmake_path(GET test STEM testname)
  add_executable(${testname} ${test})
  target_link_libraries(${testname} PUBLIC grox_debug grox_currency GTest::gtest)
  set(${testname}_PARAMETERS "${${testname}_PARAMETERS}" THREADS 0)
  grox_add_test_and_deps("util" ${testname} EXECUTABLE ${testname} ${${testname}_PARAMETERS})
endforeach()
