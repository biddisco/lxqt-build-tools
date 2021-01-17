# ------------------------------------------------------------------------------
# To build grox, the following might be necessary
# ------------------------------------------------------------------------------
# sudo apt install build-essential ibqt5charts5-dev libqt5opengl5-dev
# libqt5svg5-dev sudo apt install build-essential libhdf5-dev sudo apt install
# libboost-all-dev
#
# Build boost (1.75) on raspberry pi (4) ./bootstrap ./b2 architecture=arm
# address-model=32 threading=multi link=shared variant=release
# --prefix=$HOME/apps/boost --without-mpi --without-python --without-graph
# --without-graph_parallel -j3 install
#
cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0048 NEW)

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

set(BUILD_SHARED_LIBS ON)

enable_testing()

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
find_package(HDF5 REQUIRED COMPONENTS C)
find_package(ZLIB)

# ------------------------------------------------------------------------------
# Build qwt
# ------------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/qwt)
  message(STATUS "Using QWT already in tree  QWT library")
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

# ------------------------------------------------------------------------------
# grox build
# ------------------------------------------------------------------------------

set(GROX_SRC
    main.cpp
    mainwindow.cpp
    StackedStockCharts.cpp
    #
    internet/evp-encrypt.cpp
    #
    internet/websocket-ssl.cpp
    internet/https-async.cpp
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
    patternPlot/OrderBookPlot.cpp
    patternPlot/OrderBookCurve.cpp
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
set(GROX_QRC_SRC grox.qrc)

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
         ${HDF5_LIBRARIES}
         ${STD_FILESYSTEM_LIB}
         OpenSSL::SSL
         pthread
)

# ---------------------------------------------------
# test websocket code
# ---------------------------------------------------
set(WS_SRC internet/websocket-ssl.cpp internet/test/test-websocket.cpp)

add_executable(test-websocket ${WS_SRC})

target_include_directories(test-websocket PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(
  test-websocket PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL pthread
)

# ---------------------------------------------------
# test https code
# ---------------------------------------------------
set(HTTPS_SRC internet/https-async.cpp internet/test/test-https.cpp)

add_executable(test-https ${HTTPS_SRC})

target_include_directories(test-https PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(test-https PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL pthread)

# ---------------------------------------------------
# test AES code
# ---------------------------------------------------
set(AES256_SRC internet/evp-encrypt.cpp internet/test/test-aes256.cpp)

add_executable(test-aes256 ${AES256_SRC})

target_link_libraries(test-aes256 PUBLIC OpenSSL::SSL pthread)

# ---------------------------------------------------
# Ripple transaction signing demo
# ---------------------------------------------------
set(static OFF)
add_subdirectory(validator-keys-tool)
