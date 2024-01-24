find_package(Qt6 REQUIRED COMPONENTS Core)

set(grox_util_HDRS datetime_utils.hpp nljson.hpp stringutils.hpp)

set(grox_util_SRCS nljson.cpp)

add_library(grox_util ${grox_util_SRCS} ${grox_util_HDRS})

target_include_directories(grox_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(grox_util PUBLIC nlohmann_json Qt::Core)

if(BUILD_TESTING AND (EXISTS test))

  pika_add_pseudo_target(tests.util)
  pika_add_pseudo_dependencies(tests tests.util)
  add_subdirectory(test)
endif()
