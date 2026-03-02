# ------------------------------------------------------------------------------
# To build grox, the following might be necessary
# ------------------------------------------------------------------------------
# ~~~
# check status of qt install (last check was 6.2)
#   qtdiag
#
# Install all Qt6 dev packages:
#   sudo apt search qt6 2>&1 | grep ".*-dev" | sed 's/\/.*//' | sort -u | xargs sudo apt -y install
#
# Qt6 GUI needs opengl,
#   sudo apt install libgl1-mesa-dev
#
# Advanced docking system needs
#   sudo apt install qml6-module-qtquick qt6-declarative-dev
#
# SSL needs:
#   sudo apt-get install libssl-dev libcurl4-openssl-dev
#
# Rippled needs
#   sudo apt install liblz4-dev libarchive-dev libsoci-dev
# ~~~
#
# custom Qt installation cmake -DCMAKE_PREFIX_PATH=/usr/local/Qt-6.6.1/ -DQT_DEBUG_FIND_PACKAGE=ON
# ~/src/grox/

# ------------------------------------------------------------------------------
# CMake version requirement
# ------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

# ------------------------------------------------------------------------------
# Policies - use the latest of everything
# ------------------------------------------------------------------------------
cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})

# ------------------------------------------------------------------------------
# Project declaration
# ------------------------------------------------------------------------------
project(GROX VERSION 0.1 LANGUAGES CXX C CUDA)

# ------------------------------------------------------------------------------
# CMake configuration
# ------------------------------------------------------------------------------
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")

# ------------------------------------------------------------------------------
# CXX standard 20
# ------------------------------------------------------------------------------
set(GROX_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD ${GROX_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# PIC needed by ripple libs
# ------------------------------------------------------------------------------
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Position independent code")
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "build shared libraries" OFF)

option(GROX_WITH_PYTHON_INDICATORS "Enable Python-based dynamic indicators" ON)
option(GROX_WITH_SANITIZER "build grox with address/memory sanitizer flags" OFF)
option(GROX_WITH_TESTING_SANITIZER "build grox tests with address/memory sanitizer flags" OFF)
option(GROX_WITH_IWYU "add include-what-you-use support to grox build" OFF)

# ------------------------------------------------------------------------------
# Logging / Log level support for messages >= level
# ------------------------------------------------------------------------------
set(GROX_LOG_LEVEL "off" CACHE STRING "Enable logging for messages >=level")
set_property(
  CACHE GROX_LOG_LEVEL
  PROPERTY STRINGS
           off
           trace
           debug
           info
           warn
           error
           critical
)

# ------------------------------------------------------------------------------
# CMake modules we need to include
# ------------------------------------------------------------------------------
include(FetchContent)
include(FindPkgConfig)
include(ExternalProject)
include(Sanitizer)
include(ConfigNamespace)

# ------------------------------------------------------------------------------
# CTest testing support
# ------------------------------------------------------------------------------
set(DART_TESTING_TIMEOUT 30 CACHE STRING "Test Timeout" FORCE)
include(CTest)
enable_testing()

# ------------------------------------------------------------------------------
# GoogleTest testing support
# ------------------------------------------------------------------------------
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  set(BUILD_TESTING OFF CACHE BOOL "Testing")
endif()

# ------------------------------------------------------------------------------
# Setup Output directories
# ------------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all Executables."
)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all Libraries"
)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all static libraries."
)

# ------------------------------------------------------------------------------
# include-what-you-use
# ------------------------------------------------------------------------------
if(GROX_WITH_IWYU)
  find_program(iwyu_path NAMES include-what-you-use iwyu REQUIRED)
  message(STATUS "iwyu PATH ${iwyu_path}")
endif()

# ------------------------------------------------------------------------------
# Find Qt6
# ------------------------------------------------------------------------------
# this line to force correct var so that Qt-AdvancedDocking uses Qt6 by default
find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)
#
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

find_package(Qt6 REQUIRED COMPONENTS ${qtcomponents})

# ------------------------------------------------------------------------------
# c++26 std::execution
# ------------------------------------------------------------------------------
find_package(stdexec REQUIRED)

# ------------------------------------------------------------------------------
# range-v3 library
# ------------------------------------------------------------------------------
find_package(range-v3 REQUIRED)

# ------------------------------------------------------------------------------
# fmt formatting library
# ------------------------------------------------------------------------------
find_package(fmt REQUIRED)

# ------------------------------------------------------------------------------
# spdlog logging library
# ------------------------------------------------------------------------------
string(TOUPPER ${GROX_LOG_LEVEL} GROX_LOG_LEVEL_UPPERCASE)
if(NOT "${GROX_LOG_LEVEL_UPPERCASE}" MATCHES "OFF")
  find_package(spdlog REQUIRED)
endif()

# ------------------------------------------------------------------------------
# Boost
# ------------------------------------------------------------------------------
find_package(Boost NO_MODULE REQUIRED COMPONENTS headers program_options)

# ------------------------------------------------------------------------------
# system thread libs
# ------------------------------------------------------------------------------
find_package(Threads REQUIRED)

# ------------------------------------------------------------------------------
# OpenSSL
# ------------------------------------------------------------------------------
find_package(OpenSSL REQUIRED)

# ------------------------------------------------------------------------------
# HDF5
# ------------------------------------------------------------------------------
find_package(HDF5 REQUIRED COMPONENTS C) # needs mpi if compiled with parallel
add_library(HDF5::HDF5 ALIAS hdf5-shared)

# ------------------------------------------------------------------------------
# HighFive HDF5 library
# ------------------------------------------------------------------------------
find_package(HighFive REQUIRED)

# ------------------------------------------------------------------------------
# magic_enum
# ------------------------------------------------------------------------------
find_package(magic_enum REQUIRED)

# ------------------------------------------------------------------------------
# Find nlohmann-json
# ------------------------------------------------------------------------------
find_package(nlohmann_json 3.11.3 REQUIRED)

# ------------------------------------------------------------------------------
# Build/Download qwt
# ------------------------------------------------------------------------------
get_external_project(
  PROJECT_NAME
  "qwt"
  GIT_REPO
  "$ENV{HOME}/git-repos/qwt"
  GIT_TAG
  "candle-fixes"
)

# ------------------------------------------------------------------------------
# Build/Download Qt-Advanced-Docking-System
# ------------------------------------------------------------------------------
set(BUILD_EXAMPLES OFF)
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
# Build lxqt-build-tools (required by qtermwidget)
# ------------------------------------------------------------------------------
set(PULL_TRANSLATIONS OFF)
get_external_project(
  PROJECT_NAME
  "lxqt2-build-tools"
  FOLDER_NAME
  "lxqt-build-tools"
  GIT_REPO
  "https://github.com/lxqt/lxqt-build-tools"
  GIT_TAG
  "master"
)
set(lxqt2-build-tools_DIR "${lxqt2-build-tools_BINARY_DIR}/install")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" "${CMAKE_BINARY_DIR}")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" "${lxqt2-build-tools_DIR}")
message("lxqt2-build-tools_DIR ${lxqt2-build-tools_DIR}")

# ------------------------------------------------------------------------------
# Build qtermwidget
# ------------------------------------------------------------------------------
set(BUILD_EXAMPLE OFF CACHE BOOL "Don't build qtermwidget example")
set(UPDATE_TRANSLATIONS OFF CACHE BOOL "Don't update translations")
get_external_project(
  PROJECT_NAME
  "qtermwidget"
  GIT_REPO
  "https://github.com/lxqt/qtermwidget.git"
  GIT_TAG
  "master"
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
# Find/Build/Download rippled
# ------------------------------------------------------------------------------
set(static FALSE)
set(rocksdb FALSE)
set(unity FALSE)
set(tests "" CACHE BOOL "" FORCE)
set(protobuf_MODULE_COMPATIBLE TRUE)
set(SECP256K1_BUILD_TESTS FALSE)
set(SECP256K1_BUILD_EXHAUSTIVE_TESTS FALSE)
# To setup where Rippled puts its extern projects
set(NIH_CACHE_ROOT ${PROJECT_BINARY_DIR}/extern)
file(MAKE_DIRECTORY ${NIH_CACHE_ROOT})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/proto_gen_grpc/ripple/proto/)
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
set(PIKA_LOG_LEVEL ${GROX_LOG_LEVEL_UPPERCASE})
set(PIKA_WITH_CXX_STANDARD ${GROX_CXX_STANDARD})
set(PIKA_WITH_STDEXEC ON)
set(PIKA_WITH_SANITIZERS=${GROX_WITH_SANITIZER})
get_external_project(
  PROJECT_NAME
  "pika"
  GIT_REPO
  "https://github.com/pika-org/pika"
  GIT_TAG
  "main"
  DO_NOT_FIND
)

if(pika_WITH_APEX)
  # I think something like this should be done in APEX' CMakeLists.pika
  include_directories(
    ${pika_SOURCE_DIR}/libs/preprocessor/include ${pika_SOURCE_DIR}/apex/src/apex
    ${pika_SOURCE_DIR}/apex/src/contrib
  )
endif()

# -------------------------------------------------------------
# Find/Build/Download magic_enum
# -------------------------------------------------------------
set(MAGIC_ENUM_OPT_BUILD_EXAMPLES OFF CACHE BOOL " " FORCE)
get_external_project(
  PROJECT_NAME
  magic_enum
  GIT_REPO
  "https://github.com/Neargye/magic_enum.git"
  GIT_TAG
  "master"
)

# ------------------------------------------------------------------------------
# global include paths that subdirs will use
# ------------------------------------------------------------------------------
# config defines are written here in the binary directory
set(GROX_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/src)

# ------------------------------------------------------------------------------
# special targets for tests that are scoped by category
# ------------------------------------------------------------------------------
if(BUILD_TESTING)
  set(PIKA_WITH_PSEUDO_DEPENDENCIES ON CACHE INTERNAL "" FORCE)
  include(grox_add_test)
  pika_add_pseudo_target(tests)
endif()

# ------------------------------------------------------------------------------
# include/build all the subdirectories/modules
# ------------------------------------------------------------------------------
add_subdirectory(src/util)
add_subdirectory(src/data)
add_subdirectory(src/currency)
add_subdirectory(src/debug)
add_subdirectory(src/exchange)
add_subdirectory(src/indicators)
add_subdirectory(src/io)
add_subdirectory(src/network)
add_subdirectory(src/plot)
add_subdirectory(src/widgets_control)
add_subdirectory(src/widgets)

# ------------------------------------------------------------------------------
# include/build all the applications
# ------------------------------------------------------------------------------
add_subdirectory(src/apps)

# ------------------------------------------------------------------------------
# generate config settings headers
# ------------------------------------------------------------------------------
add_config_define_namespace(NAMESPACE ALL DEFINE GROX_SOURCE_DIR VALUE "\"${GROX_SOURCE_DIR}\"")
add_config_define_namespace(NAMESPACE ALL DEFINE GROX_BINARY_DIR VALUE "\"${GROX_BINARY_DIR}\"")

write_config_defines_file(
  NAMESPACE ALL FILENAME "${PROJECT_BINARY_DIR}/src/grox/config-defines.hpp"
)
write_config_defines_file(
  NAMESPACE LOGGING FILENAME "${PROJECT_BINARY_DIR}/src/grox/config-logging.hpp"
)
write_config_defines_file(
  NAMESPACE PYTHON FILENAME "${PROJECT_BINARY_DIR}/src/grox/config-python.hpp"
)
