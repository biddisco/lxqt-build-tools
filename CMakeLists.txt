# ------------------------------------------------------------------------------
# To build grox, the following might be necessary
# ------------------------------------------------------------------------------
# All Qt5 :
# apt search libqt5 2>&1 | grep "^libqt5.*-dev" | sed 's/\/.*//' | xargs sudo apt -y install
# sudo apt install qtquickcontrols2-5-dev
#
# sudo apt install build-essential libqt5charts5-dev libqt5opengl5-dev libqt5svg5-dev libcurl4-openssl-dev
# QPlatformNativeInterface requires base-private-dev
#
# qt6 : sudo apt install qt6-base-dev qt6-base-private-dev libqt6opengl6-dev libqt6svg6-dev
# Advanced docking system needs
# sudo apt install qml6-module-qtquick qt6-declarative-dev
#
# Assumes hdf5, boost, installed using spack environment
# Qt6 GUI needs opengl,
# sudo apt install libgl1-mesa-dev
# SSL : sudo apt-get install libssl-dev libcurl4-openssl-dev
#
# Rippled needs
# sudo apt install liblz4-dev libarchive-dev libsoci-dev
#
# Build boost (1.75) on raspberry pi (4) ./bootstrap ./b2 architecture=arm
# address-model=32 threading=multi link=shared variant=release
# --prefix=$HOME/apps/boost --without-mpi --without-python --without-graph
# --without-graph_parallel -j3 install
#
# Example cmake invocation
# cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Debug ~/src/xrp/grox/

# ------------------------------------------------------------------------------
# @TODO : add acknowledgement to
# https://github.com/sroehling/ChartPatternFinderDesktop
# ------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0144 NEW)

project(
  GROX
  VERSION 0.1
  LANGUAGES CXX C
)

# ------------------------------------------------------------------------------
# CMake configuration
# ------------------------------------------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

# ------------------------------------------------------------------------------
# CXX standard 20
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "build shared ripple libraries" OFF)
set(Boost_NO_WARN_NEW_VERSIONS 1)

option(GROX_WITH_SANITIZER "build grox using address sanitzer flags" OFF)
option(GROX_WITH_IWYU "add include-what-you-use support to grox build" OFF)

# ------------------------------------------------------------------------------
# CTest testing support
# ------------------------------------------------------------------------------
include(CTest)
enable_testing()

# ------------------------------------------------------------------------------
# GoogleTest testing support
# ------------------------------------------------------------------------------
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  set(BUILD_TESTING
      OFF
      CACHE BOOL "Testing"
  )
endif()

# ------------------------------------------------------------------------------
# CMake modules
# ------------------------------------------------------------------------------
include(FetchContent)

# ------------------------------------------------------------------------------
# Setup Output directories
# ------------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all Executables."
)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all Libraries"
)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all static libraries."
)

# ------------------------------------------------------------------------------
# Utility function to print all vars
# ------------------------------------------------------------------------------
function(dump_cmake_variables)
  get_cmake_property(_variableNames VARIABLES)
  list(SORT _variableNames)
  foreach(_variableName ${_variableNames})
    if(ARGV0)
      unset(MATCHED)
      string(REGEX MATCH ${ARGV0} MATCHED ${_variableName})
      if(NOT MATCHED)
        continue()
      endif()
    endif()
    message(STATUS "${_variableName}=${${_variableName}}")
  endforeach()
endfunction()

# ------------------------------------------------------------------------------
# FindPkg
# ------------------------------------------------------------------------------
include(FindPkgConfig)

# ------------------------------------------------------------------------------
# Find Qt, first 6, then 5 if not found
# ------------------------------------------------------------------------------
# this line to force correct var so that Qt-AdvancedDocking uses Qt6 by default
find_package(
  QT NAMES Qt6
  COMPONENTS Core
  REQUIRED
)
#
find_package(Qt6 COMPONENTS Core)
if(NOT Qt6_FOUND)
  find_package(Qt5 5.15 REQUIRED COMPONENTS Core)
endif()
message(STATUS "Grox Using Qt${QT_DEFAULT_MAJOR_VERSION}")

if(${QT_DEFAULT_MAJOR_VERSION} EQUAL 6)
  set(qtcomponents
      Core
      Widgets
      Gui
      OpenGL
      OpenGLWidgets
      PrintSupport
      Svg
      Concurrent
  )
else()
  set(qtcomponents
      Core
      Widgets
      Gui
      OpenGL
      PrintSupport
      Svg
      Concurrent
  )
endif()

find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS ${qtcomponents})

find_package(Boost NO_MODULE REQUIRED system unit_test_framework)
find_package(OpenSSL REQUIRED)
# find_package(MPI     REQUIRED COMPONENTS CXX) # needed by hdf5 if compiled
# with defaults
find_package(HDF5 REQUIRED COMPONENTS C) # needs mpi if compiled with parallel
find_package(ZLIB)
find_package(pika)
find_package(fmt)

# -------------------------------------------------------------
# get_external_project
# -------------------------------------------------------------
function(get_external_project)
  cmake_parse_arguments(
    ARGS
    "DO_NOT_BUILD" # options
    "PROJECT_NAME;GIT_REPO;GIT_TAG;FOLDER_NAME" # 1 value args
    "" # multivalued args
    ${ARGN}
  )

  string(TOUPPER ${ARGS_PROJECT_NAME} UPPER_PROJECT)
  string(TOLOWER ${ARGS_PROJECT_NAME} LOWER_PROJECT)
  find_package(${ARGS_PROJECT_NAME} QUIET)

  if(NOT ${ARGS_PROJECT_NAME}_FOUND)
    # look in the current source tree
    if(ARGS_FOLDER_NAME)
      set(LOCAL_DIR ${PROJECT_SOURCE_DIR}/extern/${ARGS_FOLDER_NAME})
    else()
      set(LOCAL_DIR ${PROJECT_SOURCE_DIR}/extern/${ARGS_PROJECT_NAME})
    endif()
    if(EXISTS ${LOCAL_DIR}/.git)
      message(STATUS "ext: " "Using ${ARGS_PROJECT_NAME} in (${LOCAL_DIR})")
      # use the source in this directory
      set(FETCHCONTENT_SOURCE_DIR_${UPPER_PROJECT} ${LOCAL_DIR})
      # don't change branches, or pull
      set(FETCHCONTENT_UPDATES_DISCONNECTED_${UPPER_PROJECT} ON)
    endif()

    fetchcontent_declare(
      ${ARGS_PROJECT_NAME}
      GIT_REPOSITORY ${ARGS_GIT_REPO}
      GIT_TAG ${ARGS_GIT_TAG}
      GIT_SHALLOW TRUE
    )
    fetchcontent_getproperties(${ARGS_PROJECT_NAME})
    if(NOT ${ARGS_PROJECT_NAME}_POPULATED)
      fetchcontent_populate(${ARGS_PROJECT_NAME})
      if(NOT ${ARGS_DO_NOT_BUILD})
        add_subdirectory(
          ${${LOWER_PROJECT}_SOURCE_DIR} ${${LOWER_PROJECT}_BINARY_DIR}
        )
      endif()
    endif()
    # make location of project visible outside function
    set(${LOWER_PROJECT}_SOURCE_DIR
        "${${LOWER_PROJECT}_SOURCE_DIR}"
        PARENT_SCOPE
    )
    set(${LOWER_PROJECT}_BINARY_DIR
        "${${LOWER_PROJECT}_BINARY_DIR}"
        PARENT_SCOPE
    )
  endif()

endfunction(get_external_project)

# ------------------------------------------------------------------------------
# Build/Download qwt
# ------------------------------------------------------------------------------
get_external_project(
  PROJECT_NAME "qwt" GIT_REPO "$ENV{HOME}/git-repos/qwt" GIT_TAG "candle-fixes"
)

# ------------------------------------------------------------------------------
# Build/Download Qt-Advanced-Docking-System
# ------------------------------------------------------------------------------
get_external_project(
  PROJECT_NAME
  "QtADS"
  FOLDER_NAME
  "Qt-Advanced-Docking-System"
  GIT_REPO
  "https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System"
  GIT_TAG
  "master"
)

# ------------------------------------------------------------------------------
# Build/Download nlohmann/json
# ------------------------------------------------------------------------------
get_external_project(
  PROJECT_NAME
  "nlohmann_json"
  FOLDER_NAME
  "json"
  GIT_REPO
  "https://github.com/nlohmann/json"
  GIT_TAG
  "develop"
)

# ------------------------------------------------------------------------------
# Build/Download belle
# ------------------------------------------------------------------------------
get_external_project(
  PROJECT_NAME
  "belle"
  FOLDER_NAME
  "belle"
  GIT_REPO
  "$ENV{HOME}/git-repos/belle"
  GIT_TAG
  "master"
  DO_NOT_BUILD
)

# ------------------------------------------------------------------------------
# Build/Download QDarkStyleSheet
# ------------------------------------------------------------------------------
get_external_project(
  PROJECT_NAME
  "QDarkStyleSheet"
  GIT_REPO
  "https://github.com/ColinDuquesnoy/QDarkStyleSheet.git"
  GIT_TAG
  "master"
  DO_NOT_BUILD
)

# ------------------------------------------------------------------------------
# Build/Download range-v3 library
# ------------------------------------------------------------------------------
get_external_project(
  PROJECT_NAME "range-v3" GIT_REPO
  "https://github.com/ericniebler/range-v3.git" GIT_TAG "master"
)

# ------------------------------------------------------------------------------
# Build/Download HighFive hdf5 c++ library
# ------------------------------------------------------------------------------
set(HIGHFIVE_UNIT_TESTS
    OFF
    CACHE STRING "HighFive unit testing"
)
set(HIGHFIVE_BUILD_DOCS OFF)

get_external_project(
  PROJECT_NAME "HighFive" GIT_REPO "https://github.com/BlueBrain/HighFive.git"
  GIT_TAG "master"
)

# ------------------------------------------------------------------------------
# Find/Build/Download rippled
# ------------------------------------------------------------------------------
set(static FALSE)
set(rocksdb FALSE)
# To setup where Rippled puts its extern projects
set(NIH_CACHE_ROOT ${PROJECT_BINARY_DIR}/extern)
file(MAKE_DIRECTORY ${NIH_CACHE_ROOT})
get_external_project(
  PROJECT_NAME
  "Ripple"
  FOLDER_NAME
  "rippled"
  GIT_REPO
  "$ENV{HOME}/git-repos/rippled"
  GIT_TAG
  "jb-master"
)

# -------------------------------------------------------------
# Find/Build/Download pika
# -------------------------------------------------------------
get_external_project(
  PROJECT_NAME "pika" GIT_REPO "https://github.com/pika-org/pika" GIT_TAG
  "main"
)
if(pika_WITH_APEX)
  # I think something like this should be done in APEX' CMakeLists.pika
  include_directories(
    ${pika_SOURCE_DIR}/libs/preprocessor/include
    ${pika_SOURCE_DIR}/apex/src/apex ${pika_SOURCE_DIR}/apex/src/contrib
  )
endif()

# -------------------------------------------------------------
# Find/Build/Download magic_enum
# -------------------------------------------------------------
set(MAGIC_ENUM_OPT_BUILD_EXAMPLES
    OFF
    CACHE BOOL " " FORCE
)
get_external_project(
  PROJECT_NAME magic_enum GIT_REPO "https://github.com/Neargye/magic_enum.git"
  GIT_TAG "master"
)
# include(properties) print_target_properties(magic_enum::magic_enum)

# ------------------------------------------------------------------------------
# grox build
# ------------------------------------------------------------------------------

add_subdirectory(src/data)
add_subdirectory(src/currency)
add_subdirectory(src/debug)
add_subdirectory(src/indicators)
add_subdirectory(src/io)

set(GROX_SRC
    # main application
    src/main.cpp
    src/mainwindow.cpp
    # src/tabbed_form.cpp application utility cryptography
    src/network/evp-encrypt.cpp
    # networking asio etc
    src/network/websocket-ssl.cpp
    src/network/https-async.cpp
    # exchange APIs
    src/exchange/xrpl_json.cpp
    src/exchange/xrpl.cpp
    src/exchange/exchange.cpp
    src/exchange/xrpl_network.cpp
    src/exchange/bitstamp.cpp
    src/exchange/order_book.cpp
    # widgets and dialogs
    src/widgets/check_trades_dialog.cpp
    src/widgets/connection_widget.cpp
    src/widgets/currency_widget.cpp
    src/widgets/digital_clock.cpp
    src/widgets/password_dialog.cpp
    src/widgets/indicator_dialog.cpp
    src/widgets/trade_widget.cpp
    src/widgets/wallet_widget.cpp
    src/widgets/xrp_functions.cpp
    src/widgets/price_chart_widget.cpp
    # plotting
    src/plot/OrderBookPlot.cpp
    src/plot/OrderBookCurve.cpp
    src/plot/ohlc_chart_curve.cpp
    src/plot/ohlc_date_scaledraw.cpp
    src/plot/ohlc_interactor.cpp
    src/plot/ohlc_price_plot.cpp
    src/plot/indicator_plot.cpp
    src/plot/timebased_chart_plot.cpp
)

qt_wrap_ui(GROX_SRC src/tabbed_form.ui)

# -----------------------------------------------------------------------------
# Qt resource files
# -----------------------------------------------------------------------------
set(GROX_QRC_SRC
    src/grox.qrc ${qdarkstylesheet_SOURCE_DIR}/qdarkstyle/dark/darkstyle.qrc
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
  grox
  PUBLIC ${Boost_INCLUDE_DIRS} ${HDF5_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR}
         ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src
         ${belle_SOURCE_DIR}
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

if(GROX_WITH_IWYU)
  find_program(iwyu_path NAMES include-what-you-use iwyu REQUIRED)
  message(STATUS "iwyu PATH ${iwyu_path}")
  set_property(
    TARGET grox PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path} -Xiwyu --verbose
  )
endif()

target_compile_definitions(
  grox
  PRIVATE # needed for UUID generation in web requests
          GROX_HAVE_UUID_ENCODING
          # disable support for belle websocket server
          OB_BELLE_CONFIG_SERVER_OFF
)

target_link_libraries(
  grox
  PUBLIC grox_data
         grox_currency
         grox_io
         qwt
         Qt::Core
         Qt::Gui
         Qt::OpenGL
         Qt::Svg
         Qt::Widgets
         $<$<EQUAL:${QT_DEFAULT_MAJOR_VERSION},6>:Qt::OpenGLWidgets>
         qt${QT_VERSION_MAJOR}advanceddocking
         ${Boost_LIBRARIES}
         nlohmann_json
         fmt::fmt
         range-v3
         Ripple::xrpl_core
         uuid
         OpenSSL::SSL
         pthread
         pika::pika
)

# ---------------------------------------------------
# Convenience var for test setup
# ---------------------------------------------------
set(TEST_INC_DIRS ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------
# Vanity address generator
# ---------------------------------------------------
# add_executable(vanity vanity/vanity.cpp) target_link_libraries(vanity range-v3
# Ripple::xrpl_core pika::pika) enable optimizations for vanity generator
# regardless of build type setting target_compile_options(vanity PRIVATE -O3)

# ---------------------------------------------------
# OLD : vanity address generator
# ---------------------------------------------------
# add_subdirectory(extern/vanity)

# ---------------------------------------------------
# test websocket code
# ---------------------------------------------------
set(WS_SRC src/network/websocket-ssl.cpp src/network/test/test-websocket.cpp)

add_executable(test-websocket ${WS_SRC})

target_include_directories(test-websocket PUBLIC ${TEST_INC_DIRS})

target_link_libraries(
  test-websocket PUBLIC ${Boost_LIBRARIES} nlohmann_json OpenSSL::SSL pthread
)

# ---------------------------------------------------
# test https code
# ---------------------------------------------------
set(HTTPS_SRC src/network/https-async.cpp src/network/test/test-https.cpp)

add_executable(test-https ${HTTPS_SRC})

target_include_directories(test-https PUBLIC ${TEST_INC_DIRS})

target_link_libraries(
  test-https PUBLIC ${Boost_LIBRARIES} nlohmann_json OpenSSL::SSL pthread
)

# ---------------------------------------------------
# test Token authentication
# ---------------------------------------------------
set(LOGIN_SRC src/network/test/test-login.cpp src/network/https-async.cpp
              src/network/evp-encrypt.cpp
)
add_executable(test-login ${LOGIN_SRC})

target_include_directories(test-login PUBLIC ${TEST_INC_DIRS})

target_link_libraries(
  test-login PUBLIC ${Boost_LIBRARIES} nlohmann_json OpenSSL::SSL curl uuid
                    pthread
)

target_compile_definitions(
  test-login PRIVATE GROX_HAVE_CURL_ENCODING GROX_HAVE_UUID_ENCODING
)

# ---------------------------------------------------
# Bitstamp official example
# ---------------------------------------------------
set(BITSTAMP_SRC src/network/test/test-bitstamp.cpp src/network/evp-encrypt.cpp)
add_executable(test-bitstamp ${BITSTAMP_SRC})

target_include_directories(
  test-bitstamp PUBLIC ${Boost_INCLUDE_DIRS} ${TEST_INC_DIRS}
)

target_link_libraries(
  test-bitstamp PUBLIC ${Boost_LIBRARIES} nlohmann_json OpenSSL::SSL curl uuid
                       pthread
)

# ---------------------------------------------------
# test AES code
# ---------------------------------------------------
set(AES256_SRC src/network/evp-encrypt.cpp src/network/test/test-aes256.cpp)

add_executable(test-aes256 ${AES256_SRC})
target_include_directories(test-aes256 PUBLIC ${TEST_INC_DIRS})
target_link_libraries(test-aes256 PUBLIC OpenSSL::SSL pthread)

# ---------------------------------------------------
# Ripple transaction signing demo
# ---------------------------------------------------
if(TARGET Ripple::xrpl_core)
  set(SIGN_SRC src/network/test/ripple-sign-demo.cpp)
  add_executable(test-ripple-signing ${SIGN_SRC})
  target_link_libraries(
    test-ripple-signing PUBLIC Ripple::xrpl_core OpenSSL::SSL pthread
  )
endif()

# ---------------------------------------------------
# belle - client test against bitstamp API
# ---------------------------------------------------
add_executable(test-belle-client src/network/test/test-belle-client.cpp)
target_include_directories(
  test-belle-client PUBLIC ${belle_SOURCE_DIR}/include ${TEST_INC_DIRS}
)
target_link_libraries(
  test-belle-client PUBLIC ssl crypto pthread ${Boost_LIBRARIES} nlohmann_json
)

# ---------------------------------------------------
# belle - client test against bitstamp API
# ---------------------------------------------------
add_executable(
  test-belle-client-request src/network/test/test-belle-client-request.cpp
)
target_include_directories(
  test-belle-client-request PUBLIC ${belle_SOURCE_DIR}/include ${TEST_INC_DIRS}
)
target_link_libraries(
  test-belle-client-request PUBLIC ssl crypto pthread ${Boost_LIBRARIES}
                                   nlohmann_json
)

# ---------------------------------------------------
# belle - websocket test against ripple API
# ---------------------------------------------------
add_executable(test-belle-websocket src/network/test/test-belle-websocket.cpp)
target_include_directories(
  test-belle-websocket PUBLIC ${belle_SOURCE_DIR}/include ${TEST_INC_DIRS}
)
target_link_libraries(
  test-belle-websocket PUBLIC ssl crypto pthread ${Boost_LIBRARIES}
                              nlohmann_json
)

# ---------------------------------------------------
# belle - example original https test client/server
# ---------------------------------------------------
add_executable(
  test-belle-https-client ${belle_SOURCE_DIR}/example/client/https/src/main.cc
)
target_include_directories(
  test-belle-https-client PUBLIC ${belle_SOURCE_DIR}/include ${TEST_INC_DIRS}
)
target_link_libraries(
  test-belle-https-client PUBLIC ssl crypto pthread ${Boost_LIBRARIES}
                                 nlohmann_json
)
target_compile_definitions(
  test-belle-https-client
  PRIVATE BELLE_CERT_PATH="${belle_SOURCE_DIR}/example/cert"
)

# add_executable(test-belle-https-server
# ${CMAKE_CURRENT_SOURCE_DIR}/belle_SOURCE_DIR/example/server/https/src/main.cc)
# target_include_directories(test-belle-https-server PUBLIC
# ${belle_SOURCE_DIR}/include ${TEST_INC_DIRS})
# target_link_libraries(test-belle-https-server PUBLIC ssl crypto pthread
# ${Boost_LIBRARIES}) target_compile_definitions(test-belle-https-server PRIVATE
# BELLE_CERT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/belle_SOURCE_DIR/example/cert" )
