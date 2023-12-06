set(grox_debug_SRCS)

add_library(grox_debug INTERFACE)

target_include_directories(
  grox_debug INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(grox_debug INTERFACE ${Boost_LIBRARIES})

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.debug)
  pika_add_pseudo_dependencies(tests tests.debug)
  add_subdirectory(test)
endif()
