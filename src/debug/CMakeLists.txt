# Currently there are no source files for this library
set(grox_debug_SRCS demangle_helper.hpp logging.hpp print.cpp print.hpp)

# debug library
add_library(grox_debug ${grox_debug_SRCS})

target_include_directories(
  grox_debug INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_include_directories(grox_debug PUBLIC ${GROX_INCLUDE_ROOT})
target_link_libraries(grox_debug PUBLIC pika::pika)

# ------------------------------------------------------------------------------
# logging support
# ------------------------------------------------------------------------------
add_config_define_namespace(NAMESPACE LOGGING DEFINE GROX_LOGGING_ENABLED VALUE "1")
add_config_define_namespace(
  NAMESPACE LOGGING UNDEFINE DEFINE SPDLOG_ACTIVE_LEVEL
  VALUE "SPDLOG_LEVEL_${GROX_LOG_LEVEL_UPPERCASE}"
)
if(NOT "${GROX_LOG_LEVEL_UPPERCASE}" MATCHES "OFF")
  target_link_libraries(grox_debug PUBLIC spdlog::spdlog fmt::fmt)
endif()

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.debug)
  pika_add_pseudo_dependencies(tests tests.debug)
  add_subdirectory(test)
endif()
