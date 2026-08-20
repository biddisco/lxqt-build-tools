find_package(SOCI REQUIRED)
find_package(SQLite3 REQUIRED)

set(grox_io_SRCS hdf5_ohlc_manager.hpp hdf5_ohlc_manager.cpp transaction_store.hpp
                 transaction_store.cpp
)

qt_add_library(grox_io STATIC ${grox_io_SRCS})

target_include_directories(grox_io PUBLIC ${SQLite3_INCLUDE_DIRS})

target_link_libraries(
  grox_io
  PUBLIC grox_data
         grox_debug
         pika::pika
         HighFive::HighFive
         SOCI::soci_core
         SOCI::soci_sqlite3
         SQLite::SQLite3
         Qt::Core
         fmt::fmt
)

if(GROX_WITH_IWYU)
  set_property(TARGET grox_io PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
endif()
