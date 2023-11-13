add_executable(debug_print debug_print.cpp)
target_include_directories(debug_print PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(debug_print PUBLIC GTest::gtest)
add_test(NAME debug_print COMMAND $<TARGET_FILE:debug_print>)
