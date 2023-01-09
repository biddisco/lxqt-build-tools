# ------------------------------------------------------------------------------
# To build grox, the following might be necessary
# ------------------------------------------------------------------------------
# sudo apt install build-essential libqt5charts5-dev libqt5opengl5-dev libqt5svg5-dev libcurl4-openssl-dev
# QPlatformNativeInterface requires base-private-dev
# qt6 : sudo apt install qt6-base-dev qt6-base-private-dev libqt6opengl6-dev libqt6svg6-dev
# Assumes hdf5, boost, installed using space environment
#
#
# Build boost (1.75) on raspberry pi (4) ./bootstrap ./b2 architecture=arm
# address-model=32 threading=multi link=shared variant=release
# --prefix=$HOME/apps/boost --without-mpi --without-python --without-graph
# --without-graph_parallel -j3 install
#
# Example cmake invocation
# cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Debug ~/src/xrp/grox/

# ------------------------------------------------------------------------------
# @TODO : add acknowledgement to https://github.com/sroehling/ChartPatternFinderDesktop
# ------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0135 NEW)

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

# ------------------------------------------------------------------------------
# Testing
# ------------------------------------------------------------------------------
enable_testing()

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
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        if (ARGV0)
            unset(MATCHED)
            string(REGEX MATCH ${ARGV0} MATCHED ${_variableName})
            if (NOT MATCHED)
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
find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)
# 
find_package(Qt6 COMPONENTS Core)
if (NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core)
endif()
message(STATUS "Grox Using Qt${QT_DEFAULT_MAJOR_VERSION}")

if (${QT_DEFAULT_MAJOR_VERSION} EQUAL 6)
    set(qtcomponents Core Widgets Gui OpenGL OpenGLWidgets PrintSupport Svg Concurrent)
else()
    set(qtcomponents Core Widgets Gui OpenGL PrintSupport Svg Concurrent)
endif()

find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS ${qtcomponents})

find_package(Boost   NO_MODULE REQUIRED system unit_test_framework)
find_package(OpenSSL REQUIRED)
find_package(MPI     REQUIRED COMPONENTS CXX) # needed by hdf5 if compiled with defaults
find_package(HDF5    REQUIRED COMPONENTS C) # needs mpi if compiled with parallel
find_package(ZLIB)
find_package(pika)

# ------------------------------------------------------------------------------
# Build/Download qwt
# ------------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/qwt)
  message(STATUS "Using QWT in source tree")
  # use the source in this directory
  set(FETCHCONTENT_SOURCE_DIR_QWT ${CMAKE_CURRENT_SOURCE_DIR}/extern/qwt)
  # don't change branches, or pull
  set(FETCHCONTENT_UPDATES_DISCONNECTED_QWT ON)
endif()

# If we must pull a fresh copy, setup git location/branch and set shallow flag
fetchcontent_declare(
  qwt
  GIT_REPOSITORY $ENV{HOME}/src/qwt
  GIT_TAG svn-cmake
  GIT_SHALLOW TRUE
)
fetchcontent_getproperties(qwt)
if(NOT qwt_POPULATED)
  message(STATUS "Downloading QWT source...")
  fetchcontent_populate(qwt)
  add_subdirectory(${qwt_SOURCE_DIR} ${qwt_BINARY_DIR})
endif()

# ------------------------------------------------------------------------------
# Build/Download Qt-Advanced-Docking-System
# ------------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/Qt-Advanced-Docking-System)
  message(STATUS "Using Qt-Advanced-Docking-System in source tree")
  # use the source in this directory
  set(FETCHCONTENT_SOURCE_DIR_QTADS ${CMAKE_CURRENT_SOURCE_DIR}/extern/Qt-Advanced-Docking-System)
  # don't change branches, or pull
  set(FETCHCONTENT_UPDATES_DISCONNECTED_QTADS ON)
endif()

# If we must pull a fresh copy, setup git location/branch and set shallow flag
fetchcontent_declare(
  QtADS
  GIT_REPOSITORY https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System
  GIT_TAG master
  GIT_SHALLOW TRUE
)
fetchcontent_getproperties(QtADS)
if(NOT QtADS_POPULATED)
  message(STATUS "Downloading QtADS source...")
  fetchcontent_populate(QtADS)
  add_subdirectory(${qtads_SOURCE_DIR} ${qtads_BINARY_DIR})
endif()

# ------------------------------------------------------------------------------
# Build/Download range-v3 library
# ------------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/range-v3)
  message(STATUS "Using Range-v3 in source tree")
  # use the source in this directory
  set(FETCHCONTENT_SOURCE_DIR_RANGE-V3 ${CMAKE_CURRENT_SOURCE_DIR}/extern/range-v3)
  # don't change branches, or pull
  set(FETCHCONTENT_UPDATES_DISCONNECTED_RANGE-V3 ON)
endif()

# If we must pull a fresh copy, setup git location/branch and set shallow flag
fetchcontent_declare(
  range-v3
  GIT_REPOSITORY https://github.com/ericniebler/range-v3.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
fetchcontent_getproperties(range-v3)
if(NOT range-v3_POPULATED)
  message(STATUS "Downloading range-v3 source...")
  fetchcontent_populate(range-v3)
  add_subdirectory(${range-v3_SOURCE_DIR} ${range-v3_BINARY_DIR})
endif()

# ------------------------------------------------------------------------------
# Find/Build/Download rippled
# ------------------------------------------------------------------------------
# try to find a system version
find_package(Ripple QUIET)

if(NOT Ripple_FOUND)
  # flag tells rippled not to link to static boost/etc
  set(static FALSE)
  # look in the current source tree
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/rippled/.git)
    message(STATUS "Using rippled in source tree (${CMAKE_CURRENT_SOURCE_DIR}/extern/rippled)")
    # use the source in this directory
    set(FETCHCONTENT_SOURCE_DIR_RIPPLED ${CMAKE_CURRENT_SOURCE_DIR}/extern/rippled)
    # don't change branches, or pull
    set(FETCHCONTENT_UPDATES_DISCONNECTED_RIPPLED ON)
    # To setup where Rippled puts its extern projects
    set(NIH_CACHE_ROOT ${PROJECT_BINARY_DIR}/extern)
  endif()

  # If we must pull a fresh copy, setup git location/branch and set shallow flag
  fetchcontent_declare(
    rippled
    GIT_REPOSITORY https://github.com/ripple/rippled.git
    GIT_TAG master
    GIT_SHALLOW TRUE
  )
  fetchcontent_getproperties(rippled)
  if(NOT rippled_POPULATED)
    message(STATUS "Downloading rippled source...")
    fetchcontent_populate(rippled)
    add_subdirectory(${rippled_SOURCE_DIR} ${rippled_BINARY_DIR})
  endif()
endif()

# -------------------------------------------------------------
# Find/Build/Download pika
# -------------------------------------------------------------
find_package(pika QUIET)

if(NOT pika_FOUND)
  # look in the current source tree
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/pika/.git)
    message(STATUS "Using pika in source tree (${CMAKE_CURRENT_SOURCE_DIR}/extern/pika)")
    # use the source in this directory
    set(FETCHCONTENT_SOURCE_DIR_PIKA ${CMAKE_CURRENT_SOURCE_DIR}/extern/pika)
    # don't change branches, or pull
    set(FETCHCONTENT_UPDATES_DISCONNECTED_PIKA ON)
  endif()

  fetchcontent_declare(
    pika
    GIT_REPOSITORY https://github.com/STEllAR-GROUP/pika
    GIT_TAG master
    GIT_SHALLOW TRUE
  )
  fetchcontent_getproperties(pika)
  if(NOT pika_POPULATED)
    fetchcontent_populate(pika)
    if(pika_WITH_APEX)
      # I think something like this should be done in APEX' CMakeLists.pika
      include_directories(
        ${pika_SOURCE_DIR}/libs/preprocessor/include
        ${pika_SOURCE_DIR}/apex/src/apex ${pika_SOURCE_DIR}/apex/src/contrib
      )
    endif()
    add_subdirectory(${pika_SOURCE_DIR} ${pika_BINARY_DIR})
  endif()
endif()

# ------------------------------------------------------------------------------
# grox build
# ------------------------------------------------------------------------------

set(GROX_SRC

    # main application
    src/main.cpp
    src/mainwindow.cpp
#    src/tabbed_form.cpp

    # application utility
    src/currency.cpp
    src/json_types.cpp
    src/order_book.cpp

    # cryptography
    src/network/evp-encrypt.cpp

    # networking asio etc
    src/network/websocket-ssl.cpp
    src/network/https-async.cpp

    # data, hdf5
    src/data/ohlc_dataset_manager.cpp
    src/data/ohlc_datasets.cpp

    # exchange APIs
    src/exchange/xrpl.cpp
    src/exchange/exchange.cpp
    src/exchange/xrpl_network.cpp
    src/exchange/bitstamp.cpp

    # widgets and dialogs
    src/widgets/check_trades_dialog.cpp
    src/widgets/connection_widget.cpp
    src/widgets/currency_widget.cpp
    src/widgets/digital_clock.cpp
    src/widgets/password_dialog.cpp
    src/widgets/trade_algorithm.cpp
    src/widgets/trade_widget.cpp
    src/widgets/wallet_widget.cpp
    src/widgets/xrp_functions.cpp
    src/price_chart_widget.cpp

    # plotting
    src/plot/OrderBookPlot.cpp
    src/plot/OrderBookCurve.cpp
    src/plot/ohlc_chart_curve.cpp
    src/plot/ohlc_date_scaledraw.cpp
    src/plot/ohlc_interactor.cpp
    src/plot/ohlc_price_plot.cpp
    src/plot/filter_plot.cpp

    # utils
    src/util/QDateHelper.cpp

    src/stream/trade_filter.cpp
)

qt_wrap_ui(GROX_SRC src/tabbed_form.ui)

# -----------------------------------------------------------------------------
# Qt resource files
# -----------------------------------------------------------------------------
set(GROX_QRC_SRC src/grox.qrc)

add_executable(grox ${GROX_SRC} ${GROX_QRC_SRC})
set_property(TARGET grox PROPERTY CXX_STANDARD 20)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set_property(TARGET grox PROPERTY AUTOMOC ON)
set_property(TARGET grox PROPERTY AUTOUIC ON)
set_property(TARGET grox PROPERTY AUTORCC ON)

target_include_directories(
  grox
  PUBLIC ${Boost_INCLUDE_DIRS}
         ${HDF5_INCLUDE_DIRS}
         ${CMAKE_CURRENT_SOURCE_DIR}/extern # nlohmann/json
)

if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  set(STD_FILESYSTEM_LIB "stdc++fs")
endif()

target_compile_definitions(grox
  PRIVATE
    # needed for UUID generation in web requests
    GROX_HAVE_UUID_ENCODING
    # disable support for belle websocket server
    OB_BELLE_CONFIG_SERVER_OFF
)

target_link_libraries(
  grox
  PUBLIC qwt
         Qt::Core
         Qt::Widgets
         Qt::Gui
         Qt::OpenGL
         $<$<EQUAL:${QT_DEFAULT_MAJOR_VERSION},6>:Qt::OpenGLWidgets>
         Qt::PrintSupport
         Qt::Svg
         Qt::Concurrent
         qtadvanceddocking
         ${Boost_LIBRARIES}
         MPI::MPI_CXX
         ${HDF5_LIBRARIES}
         range-v3
         ${STD_FILESYSTEM_LIB}
         Ripple::xrpl_core
         uuid
         OpenSSL::SSL
         pthread
)

# ---------------------------------------------------
# Convenience var for test setup
# ---------------------------------------------------
set(TEST_INC_DIRS
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/extern # nlohmann/json
)

# ---------------------------------------------------
# Vanity address generator
# ---------------------------------------------------
add_executable(vanity vanity/vanity.cpp)
target_link_libraries(vanity
    range-v3
    Ripple::xrpl_core
    pika::pika
)
# enable optimizations for vanity generator regardless of build type setting
target_compile_options(vanity PRIVATE -O3)

# ---------------------------------------------------
# OLD : vanity address generator
# ---------------------------------------------------
add_subdirectory(extern/vanity)

# ---------------------------------------------------
# test websocket code
# ---------------------------------------------------
set(WS_SRC src/network/websocket-ssl.cpp src/network/test/test-websocket.cpp)

add_executable(test-websocket ${WS_SRC})

target_include_directories(test-websocket PUBLIC ${TEST_INC_DIRS})

target_link_libraries(
  test-websocket PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL pthread
)

# ---------------------------------------------------
# test https code
# ---------------------------------------------------
set(HTTPS_SRC src/network/https-async.cpp src/network/test/test-https.cpp)

add_executable(test-https ${HTTPS_SRC})

target_include_directories(test-https PUBLIC ${TEST_INC_DIRS})

target_link_libraries(test-https PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL pthread)

# ---------------------------------------------------
# test Token authentication
# ---------------------------------------------------
set(LOGIN_SRC src/network/test/test-login.cpp src/network/https-async.cpp
              src/network/evp-encrypt.cpp
)
add_executable(test-login ${LOGIN_SRC})

target_include_directories(test-login PUBLIC ${TEST_INC_DIRS})

target_link_libraries(
  test-login PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL curl uuid pthread
)

target_compile_definitions(
  test-login PRIVATE GROX_HAVE_CURL_ENCODING GROX_HAVE_UUID_ENCODING
)

# ---------------------------------------------------
# Bitstamp official example
# ---------------------------------------------------
set(BITSTAMP_SRC src/network/test/test-bitstamp.cpp src/network/evp-encrypt.cpp)
add_executable(test-bitstamp ${BITSTAMP_SRC})

target_include_directories(test-bitstamp PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(
  test-bitstamp PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL curl uuid pthread
)

# ---------------------------------------------------
# test AES code
# ---------------------------------------------------
set(AES256_SRC src/network/evp-encrypt.cpp src/network/test/test-aes256.cpp)

add_executable(test-aes256 ${AES256_SRC})

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
target_include_directories(test-belle-client PUBLIC extern/belle/include ${TEST_INC_DIRS})
target_link_libraries(test-belle-client PUBLIC ssl crypto pthread ${Boost_LIBRARIES})

# ---------------------------------------------------
# belle - client test against bitstamp API
# ---------------------------------------------------
add_executable(test-belle-client-request src/network/test/test-belle-client-request.cpp)
target_include_directories(test-belle-client-request PUBLIC extern/belle/include ${TEST_INC_DIRS})
target_link_libraries(test-belle-client-request PUBLIC ssl crypto pthread ${Boost_LIBRARIES})

# ---------------------------------------------------
# belle - websocket test against ripple API
# ---------------------------------------------------
add_executable(test-belle-websocket src/network/test/test-belle-websocket.cpp)
target_include_directories(test-belle-websocket PUBLIC extern/belle/include ${TEST_INC_DIRS})
target_link_libraries(test-belle-websocket PUBLIC ssl crypto pthread ${Boost_LIBRARIES})

# ---------------------------------------------------
# belle - example original https test client/server
# ---------------------------------------------------
add_executable(test-belle-https-client ${CMAKE_CURRENT_SOURCE_DIR}/extern/belle/example/client/https/src/main.cc)
target_include_directories(test-belle-https-client PUBLIC extern/belle/include ${TEST_INC_DIRS})
target_link_libraries(test-belle-https-client PUBLIC ssl crypto pthread ${Boost_LIBRARIES})
target_compile_definitions(test-belle-https-client
    PRIVATE BELLE_CERT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/extern/belle/example/cert"
)

#add_executable(test-belle-https-server ${CMAKE_CURRENT_SOURCE_DIR}/extern/belle/example/server/https/src/main.cc)
#target_include_directories(test-belle-https-server PUBLIC extern/belle/include ${TEST_INC_DIRS})
#target_link_libraries(test-belle-https-server PUBLIC ssl crypto pthread ${Boost_LIBRARIES})
#target_compile_definitions(test-belle-https-server
#    PRIVATE BELLE_CERT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/extern/belle/example/cert"
#)
