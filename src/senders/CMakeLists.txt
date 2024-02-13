# Currently there are no source files for this library
set(grox_senders_SRCS)
set(grox_senders_HDRS qt_helpers.hpp transfer_qt_pika.hpp trigger_qt.hpp)

# debug library is header only, so declare an interface library
add_library(grox_senders INTERFACE ${grox_senders_HDRS})

target_include_directories(
  grox_senders INTERFACE ${ CMAKE_CURRENT_SOURCE_DIR} ${
                         CMAKE_CURRENT_SOURCE_DIR} / ..)
target_link_libraries(grox_senders INTERFACE pika::pika)

if(BUILD_TESTING)
  pika_add_pseudo_target(tests.senders)
  pika_add_pseudo_dependencies(tests tests.senders)
  add_subdirectory(test)
endif()
