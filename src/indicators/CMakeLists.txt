set(grox_indicators_SRCS)

# add_library(grox_indicators ${grox_indicators_SRCS})
# target_include_directories(grox_indicators PUBLIC
# ${CMAKE_CURRENT_SOURCE_DIR}/..)

# target_link_libraries( grox_indicators PUBLIC qwt ${Boost_LIBRARIES}

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
