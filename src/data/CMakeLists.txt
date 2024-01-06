set(grox_data_SRCS ohlc_datasets.cpp ohlc_dataset_view.cpp
                   timebased_chart_data.cpp)

add_library(grox_data ${grox_data_SRCS})
target_include_directories(grox_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(grox_data PUBLIC qwt magic_enum::magic_enum
                                       ${Boost_LIBRARIES} range-v3)

if(GROX_WITH_IWYU)
  set_property(TARGET grox_data PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
endif()
