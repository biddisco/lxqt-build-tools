set(TEST_SRCS dynamic_form.cpp indicator_form.cpp python_indicator_form.cpp)

foreach(test ${TEST_SRCS})
  cmake_path(GET test STEM testname)
  add_executable(${testname} ${test})
  target_link_libraries(${testname} PUBLIC grox_widgets_control GTest::gtest)
  set(${testname}_PARAMETERS "${${testname}_PARAMETERS}" THREADS 0)
  grox_add_test_and_deps(
    "widgets_control" ${testname} EXECUTABLE ${testname} ${${testname}_PARAMETERS}
  )
endforeach()
