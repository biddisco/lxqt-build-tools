# Currently there are no source files for this library
set(grox_debug_SRCS demangle_helper.hpp print.hpp)

# debug library is header only, so declare an interface library
add_library(grox_debug INTERFACE)

target_include_directories(grox_debug INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                                                ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(grox_debug INTERFACE pika::pika)

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.debug)
  pika_add_pseudo_dependencies(tests tests.debug)
  add_subdirectory(test)
endif()
