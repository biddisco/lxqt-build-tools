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
set_property(TARGET grox PROPERTY CXX_STANDARD 20)

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
  target_compile_options(
    grox
    PRIVATE -fsanitize=address
            -fsanitize=undefined
            -fno-sanitize-recover=all
            -fsanitize=float-divide-by-zero
            -fsanitize=float-cast-overflow
            -fno-sanitize=null
            -fno-sanitize=alignment
  )
  target_link_options(
    grox
    PRIVATE
    -fsanitize=address
    -fsanitize=undefined
    -fno-sanitize-recover=all
    -fsanitize=float-divide-by-zero
    -fsanitize=float-cast-overflow
    -fno-sanitize=null
    -fno-sanitize=alignment
  )
endif()

target_compile_definitions(
  grox PRIVATE # needed for UUID generation in web requests
               GROX_HAVE_UUID_ENCODING
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
         ${Boost_LIBRARIES}
         nlohmann_json::nlohmann_json
         STDEXEC::stdexec
         fmt::fmt
         range-v3
         xrpl.libxrpl
         uuid
         Threads::Threads
         pika::pika
)
