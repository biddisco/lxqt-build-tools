# ------------------------------------------------------------------------------
# To build grox, the following might be necessary
# ------------------------------------------------------------------------------
# sudo apt install build-essential libqt5charts5-dev libqt5opengl5-dev libqt5svg5-dev
# libhdf5-dev libboost-all-dev
#
# Build boost (1.75) on raspberry pi (4) ./bootstrap ./b2 architecture=arm
# address-model=32 threading=multi link=shared variant=release
# --prefix=$HOME/apps/boost --without-mpi --without-python --without-graph
# --without-graph_parallel -j3 install
#
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)

project(
  GROX
  VERSION 0.1
  LANGUAGES CXX C
)

# ------------------------------------------------------------------------------
# CXX standard 17
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "build shared ripple libraries" OFF)

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
# FIND : Qt5
# ------------------------------------------------------------------------------
find_package(
  Qt5 REQUIRED QUIET
  COMPONENTS Core
             Widgets
             Gui
             OpenGL
             PrintSupport
             Svg
             Concurrent
)
find_package(Boost REQUIRED system unit_test_framework)
find_package(OpenSSL REQUIRED)
find_package(MPI  REQUIRED COMPONENTS CXX) # needed by hdf5 if compiled with defaults
find_package(HDF5 REQUIRED COMPONENTS C) # needs mpi if compiled with parallel
find_package(ZLIB)

# ------------------------------------------------------------------------------
# Build/Download qwt
# ------------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/qwt)
  message(STATUS "Using QWT in source tree")
  set(FETCHCONTENT_SOURCE_DIR_QWT ${CMAKE_CURRENT_SOURCE_DIR}/qwt)
  set(FETCHCONTENT_UPDATES_DISCONNECTED_QWT ON)
endif()

fetchcontent_declare(
  qwt
  GIT_REPOSITORY $HOME/src/qwt
  GIT_TAG svn-cmake
)
fetchcontent_getproperties(qwt)
if(NOT qwt_POPULATED)
  fetchcontent_populate(qwt)
  add_subdirectory(${qwt_SOURCE_DIR} ${qwt_BINARY_DIR})
endif()

add_subdirectory(extern/range-v3)


# ------------------------------------------------------------------------------
# Find/Build : Rippled library
# ------------------------------------------------------------------------------
# try to find a system version
find_package(Ripple QUIET)

if(NOT Ripple_FOUND)
  # flag tells rippled not to link to static boost/etc
  set(static FALSE)
  # look in the current source tree
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rippled/.git)
    message(STATUS "Using rippled in source tree (${CMAKE_CURRENT_SOURCE_DIR}/rippled)")
    # use the source in this directory
    set(FETCHCONTENT_SOURCE_DIR_RIPPLED ${CMAKE_CURRENT_SOURCE_DIR}/rippled)
    # don't change branches, or pull
    set(FETCHCONTENT_UPDATES_DISCONNECTED_RIPPLED ON)
  endif()
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

# ------------------------------------------------------------------------------
# grox build
# ------------------------------------------------------------------------------

set(GROX_SRC
    src/main.cpp
    src/mainwindow.cpp
    src/password_dialog.cpp
    #
    src/network/evp-encrypt.cpp
    #
    src/network/websocket-ssl.cpp
    src/network/https-async.cpp
    #
    src/currency.cpp
    src/ohlc.cpp
    src/order_book.cpp
    #
    src/exchange/xrpl.cpp
    src/exchange/exchange.cpp
    src/exchange/xrpl_network.cpp
    src/exchange/bitstamp.cpp
    #
    src/currency_widget.cpp
    src/wallet_widget.cpp
    #
    #
    chartSegment/ANDSegmentConstraint.cpp
    chartSegment/ChartSegment.cpp
    chartSegment/SegmentValsCloseToLinearEq.cpp
    chartSegment/SlopeWithinRangeConstraint.cpp
    #
    date/DateValue.cpp
    date/TimeHelper.cpp
    #
    math/DoubleRange.cpp
    math/LinearEquation.cpp
    math/MathHelper.cpp
    math/UnsignedIntRange.cpp
    math/XYCoord.cpp
    #
    plots/OrderBookPlot.cpp
    plots/OrderBookCurve.cpp
    plots/CombinedPriceVolumeCharts.cpp
    # patternMatch/AscendingTrianglePatternMatch.cpp
    # patternMatch/CupPatternMatch.cpp
    # patternMatch/CupWithHandlePatternMatch.cpp
    # patternMatch/CupWithoutHandlePatternMatch.cpp
    # patternMatch/DescendingTrianglePatternMatch.cpp
    # patternMatch/DoubleBottomPatternMatch.cpp
    # patternMatch/FallingWedgePatternMatch.cpp
    # patternMatch/PatternMatchBreakoutInfo.cpp patternMatch/PatternMatch.cpp
    # patternMatch/PatternMatchVisitor.cpp
    # patternMatch/RectanglePatternMatch.cpp
    # patternMatch/RisingWedgePatternMatch.cpp
    # patternMatch/SymetricTrianglePatternMatch.cpp
    # patternMatch/TrianglePatternMatch.cpp patternMatch/VPatternMatch.cpp
    # patternMatch/WedgePatternMatch.cpp
    #
    patternPlot/BreakdownPlotMarker.cpp
    patternPlot/BreakoutPlotMarker.cpp
    patternPlot/PatternPlotCurve.cpp
    patternPlot/PriceAndPatternPlot.cpp
    patternPlot/PseudoTimeOHLCSample.cpp
    patternPlot/StockChartDateScaleDraw.cpp
    patternPlot/OHLCCurve.cpp
    patternPlot/StockChartPlotZoomer.cpp
    #
    patternShape/PatternShape.cpp
    # patternShape/PatternShapeCurveGenerator.cpp
    # patternShape/PatternShapeGenerator.cpp
    patternShape/PatternShapePoint.cpp
    #
    quoteData/PeriodVal.cpp
    quoteData/PeriodValSegment.cpp
    quoteData/PeriodValueRef.cpp
    quoteData/PerValCltnSlidingWindow.cpp
    #
    util/QDateHelper.cpp
    util/SettingsHelper.cpp
    #
    volumePlot/VolumePlot.cpp
    volumePlot/VolumePlotCurve.cpp
    volumePlot/VolumeYAxisScaleDraw.cpp
)

# -----------------------------------------------------------------------------
# Qt resource files
# -----------------------------------------------------------------------------
set(GROX_QRC_SRC src/grox.qrc)

add_executable(grox ${GROX_SRC} ${GROX_QRC_SRC})
set_property(TARGET grox PROPERTY CXX_STANDARD 17)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set_property(TARGET grox PROPERTY AUTOMOC ON)
set_property(TARGET grox PROPERTY AUTOUIC ON)
set_property(TARGET grox PROPERTY AUTORCC ON)

target_include_directories(
  grox
  PUBLIC ${Boost_INCLUDE_DIRS}
         ${HDF5_INCLUDE_DIRS}
         ${CMAKE_CURRENT_SOURCE_DIR}/date
         ${CMAKE_CURRENT_SOURCE_DIR}/math
         ${CMAKE_CURRENT_SOURCE_DIR}/log
         ${CMAKE_CURRENT_SOURCE_DIR}/util
         ${CMAKE_CURRENT_SOURCE_DIR}/quoteData
         ${CMAKE_CURRENT_SOURCE_DIR}/patternPlot
         ${CMAKE_CURRENT_SOURCE_DIR}/volumePlot
         ${CMAKE_CURRENT_SOURCE_DIR}/patternMatch
         ${CMAKE_CURRENT_SOURCE_DIR}/patternShape
         ${CMAKE_CURRENT_SOURCE_DIR}/chartSegment
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
         Qt5::Core
         Qt5::Widgets
         Qt5::Gui
         Qt5::OpenGL
         Qt5::PrintSupport
         Qt5::Svg
         Qt5::Concurrent
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
# test websocket code
# ---------------------------------------------------
set(WS_SRC src/network/websocket-ssl.cpp src/network/test/test-websocket.cpp)

add_executable(test-websocket ${WS_SRC})

target_include_directories(test-websocket PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(
  test-websocket PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL pthread
)

# ---------------------------------------------------
# test https code
# ---------------------------------------------------
set(HTTPS_SRC src/network/https-async.cpp src/network/test/test-https.cpp)

add_executable(test-https ${HTTPS_SRC})

target_include_directories(test-https PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(test-https PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL pthread)

# ---------------------------------------------------
# test Token authentication
# ---------------------------------------------------
set(LOGIN_SRC src/network/test/test-login.cpp src/network/https-async.cpp
              src/network/evp-encrypt.cpp
)
add_executable(test-login ${LOGIN_SRC})

target_include_directories(test-login PUBLIC ${Boost_INCLUDE_DIRS})

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
target_include_directories(test-belle-client PUBLIC extern/belle/include ${Boost_INCLUDE_DIRS})
target_link_libraries(test-belle-client PUBLIC ssl crypto pthread ${Boost_LIBRARIES})

# ---------------------------------------------------
# belle - client test against bitstamp API
# ---------------------------------------------------
add_executable(test-belle-client-request src/network/test/test-belle-client-request.cpp)
target_include_directories(test-belle-client-request PUBLIC extern/belle/include ${Boost_INCLUDE_DIRS})
target_link_libraries(test-belle-client-request PUBLIC ssl crypto pthread ${Boost_LIBRARIES})

# ---------------------------------------------------
# belle - websocket test against ripple API
# ---------------------------------------------------
add_executable(test-belle-websocket src/network/test/test-belle-websocket.cpp)
target_include_directories(test-belle-websocket PUBLIC extern/belle/include ${Boost_INCLUDE_DIRS})
target_link_libraries(test-belle-websocket PUBLIC ssl crypto pthread ${Boost_LIBRARIES})

# ---------------------------------------------------
# belle - example original https test client/server
# ---------------------------------------------------
add_executable(test-belle-https-client ${CMAKE_CURRENT_SOURCE_DIR}/extern/belle/example/client/https/src/main.cc)
target_include_directories(test-belle-https-client PUBLIC extern/belle/include ${Boost_INCLUDE_DIRS})
target_link_libraries(test-belle-https-client PUBLIC ssl crypto pthread ${Boost_LIBRARIES})
target_compile_definitions(test-belle-https-client
    PRIVATE BELLE_CERT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/extern/belle/example/cert"
)

add_executable(test-belle-https-server ${CMAKE_CURRENT_SOURCE_DIR}/extern/belle/example/server/https/src/main.cc)
target_include_directories(test-belle-https-server PUBLIC extern/belle/include ${Boost_INCLUDE_DIRS})
target_link_libraries(test-belle-https-server PUBLIC ssl crypto pthread ${Boost_LIBRARIES})
target_compile_definitions(test-belle-https-server
    PRIVATE BELLE_CERT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/extern/belle/example/cert"
)
