set(grox_indicators_SRCS)

add_library(grox_indicators INTERFACE)

target_include_directories(
  grox_indicators INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                            ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(grox_indicators INTERFACE qwt grox_data grox_io fmt::fmt
                                                ${Boost_LIBRARIES})

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.indicators)
  pika_add_pseudo_dependencies(tests tests.indicators)
  add_subdirectory(test)
endif()
