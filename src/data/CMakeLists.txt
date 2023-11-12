set(grox_data_SRCS ohlc_datasets.cpp ohlc_dataset_view.cpp
                   timebased_chart_data.cpp
)

add_library(grox_data ${grox_data_SRCS})
target_include_directories(grox_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(grox_data PUBLIC qwt ${Boost_LIBRARIES} range-v3)
