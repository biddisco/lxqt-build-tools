set(grox_data_SRCS currency.cpp json_data_types.cpp)

add_library(grox_currency ${grox_data_SRCS})
target_include_directories(grox_currency PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(grox_currency PUBLIC qwt Qt::Core nlohmann_json
                                           ${Boost_LIBRARIES} range-v3)

if(GROX_WITH_IWYU)
  set_property(TARGET grox_currency PROPERTY CXX_INCLUDE_WHAT_YOU_USE
                                             ${iwyu_path})
endif()
