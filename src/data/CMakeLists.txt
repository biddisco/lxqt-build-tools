set(grox_data_SRCS
    abstract_data_manager.hpp
    ohlc_data_exception.hpp
    ohlc_data_resolutions.hpp
    ohlc_dataset.cpp
    ohlc_dataset.hpp
    ohlc_dataset_view.cpp
    ohlc_dataset_view.hpp
    ohlc_heikin_ashi.hpp
    ohlc_utils.hpp
    order_book.hpp
    order_book.cpp
    timebased_chart_data.cpp
    timebased_chart_data.hpp
)

add_library(grox_data ${grox_data_SRCS})
target_include_directories(grox_data PUBLIC ${GROX_INCLUDE_ROOT})

target_link_libraries(
  grox_data
  PUBLIC qwt
         Qt::Core
         grox_debug
         nlohmann_json::nlohmann_json
         magic_enum::magic_enum
         ${Boost_LIBRARIES}
         range-v3
)

if(GROX_WITH_IWYU)
  set_property(TARGET grox_data PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
endif()
