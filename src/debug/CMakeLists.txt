# Currently there are no source files for this library
set(grox_debug_SRCS demangle_helper.hpp print.cpp print.hpp)

# debug library is header only, so declare an interface library
add_library(grox_debug ${grox_debug_SRCS})

target_include_directories(
  grox_debug INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_include_directories(grox_debug PUBLIC ${GROX_INCLUDE_ROOT})
target_link_libraries(grox_debug INTERFACE pika::pika)

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.debug)
  pika_add_pseudo_dependencies(tests tests.debug)
  add_subdirectory(test)
endif()
