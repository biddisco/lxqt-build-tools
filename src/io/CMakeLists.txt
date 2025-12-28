set(grox_io_SRCS hdf5_ohlc_manager.hpp hdf5_ohlc_manager.cpp)

add_library(grox_io ${grox_io_SRCS})

target_link_libraries(grox_io PUBLIC grox_data grox_debug pika::pika HighFive::HighFive)

if(GROX_WITH_IWYU)
  set_property(TARGET grox_io PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
endif()
