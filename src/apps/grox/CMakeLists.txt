set(GROX_SRC # main application
    main.cpp mainwindow.cpp
)

# -----------------------------------------------------------------------------
# Qt resource files
# -----------------------------------------------------------------------------
set(GROX_QRC_SRC ${qdarkstylesheet_SOURCE_DIR}/qdarkstyle/dark/darkstyle.qrc
                 ${qdarkstylesheet_SOURCE_DIR}/qdarkstyle/light/lightstyle.qrc
)

add_executable(grox ${GROX_SRC} ${GROX_QRC_SRC})

# c++ properties
set_property(TARGET grox PROPERTY CXX_STANDARD ${GROX_CXX_STANDARD})

# Qt properties
set_property(TARGET grox PROPERTY AUTOMOC ON)
set_property(TARGET grox PROPERTY AUTOUIC ON)
set_property(TARGET grox PROPERTY AUTORCC ON)

target_include_directories(
  grox PUBLIC ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
              ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  set(STD_FILESYSTEM_LIB "stdc++fs")
endif()

if(GROX_WITH_SANITIZER)
  target_add_sanitizer_flags(grox)
  message("Do not forget to use \necho 0 | sudo tee /proc/sys/kernel/randomize_va_space")
endif()

target_compile_definitions(
  grox PRIVATE # needed for UUID generation in web requests
               GROX_HAVE_UUID_ENCODING GROX_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(
  grox
  PUBLIC grox_currency
         grox_data
         grox_debug
         grox_exchange
         grox_indicators
         grox_io
         grox_network
         grox_plot
         grox_util
         grox_widgets
         qwt
         Qt::Core
         Qt::Gui
         Qt::OpenGL
         Qt::Svg
         Qt::Widgets
         Qt::OpenGLWidgets
         qtadvanceddocking-qt${QT_VERSION_MAJOR}
         qtermwidget6
         nlohmann_json::nlohmann_json
         STDEXEC::stdexec
         fmt::fmt
         range-v3
         xrpl.libxrpl
         uuid
         Threads::Threads
         pika::pika
)
