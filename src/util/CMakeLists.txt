find_package(Qt6 REQUIRED COMPONENTS Core)

set(grox_util_SRCS datetime_utils.hpp execute_os_command.hpp json_qstring.hpp json_qstring.cpp
                   stringutils.hpp
)

add_library(grox_util ${grox_util_SRCS})

target_include_directories(grox_util PUBLIC ${GROX_INCLUDE_ROOT})
target_link_libraries(grox_util PUBLIC nlohmann_json::nlohmann_json pika::pika range-v3 Qt::Core)

if(BUILD_TESTING AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/test))
  pika_add_pseudo_target(tests.util)
  pika_add_pseudo_dependencies(tests tests.util)
  add_subdirectory(test)
endif()
