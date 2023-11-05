set(grox_io_SRCS hdf5_ohlc_manager.cpp)

add_library(grox_io ${grox_io_SRCS})

target_link_libraries(grox_io PUBLIC grox_data HighFive)
