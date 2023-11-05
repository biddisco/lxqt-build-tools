add_executable(indicator_check indicator_check.cpp)
target_link_libraries(indicator_check PUBLIC grox_data grox_io)
target_include_directories(indicator_check PUBLIC ${PROJECT_SOURCE_DIR}/src)
add_test(NAME indicator_check COMMAND $<TARGET_FILE:indicator_check>)
