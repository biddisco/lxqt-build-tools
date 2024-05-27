#
# * Find xxHash Find xxHash includes and library
#
# xxHash_INCLUDE_DIRS - where to find xxHash.h, etc. xxHash_LIBRARIES    - List of libraries when
# using xxHash. xxHash_FOUND        - True if xxHash found. xxHash_DLL_DIR      - (Windows) Path to
# the xxHash DLL xxHash_DLL          - (Windows) Name of the xxHash DLL

if(NOT
   WIN32
)
  find_package(PkgConfig)
  pkg_search_module(
    xxHash
    xxHash
    libxxHash
  )
endif()

find_path(
  xxHash_INCLUDE_DIR
  NAMES xxhash.h
  HINTS "${xxHash_INCLUDEDIR}"
        "${xxHash_HINTS}/include"
  PATHS /usr/local/include
        /usr/include
)

find_library(
  xxHash_LIBRARY
  NAMES xxhash
        libxxhash
  HINTS "${xxHash_LIBDIR}"
        "${xxHash_HINTS}/lib"
  PATHS /usr/local/lib
        /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  xxHash
  DEFAULT_MSG
  xxHash_LIBRARY
  xxHash_INCLUDE_DIR
)

if(xxHash_FOUND)
  include(CheckIncludeFile)
  include(CMakePushCheckState)

  set(xxHash_INCLUDE_DIRS ${xxHash_INCLUDE_DIR})
  set(xxHash_LIBRARIES ${xxHash_LIBRARY})

  cmake_push_check_state()
  set(CMAKE_REQUIRED_INCLUDES ${xxHash_INCLUDE_DIRS})
  check_include_file(
    xxHashframe.h
    HAVE_xxHashFRAME_H
  )
  cmake_pop_check_state()

  if(WIN32)
    set(xxHash_DLL_DIR
        "${xxHash_HINTS}/bin"
        CACHE PATH
              "Path to xxHash DLL"
    )
    file(
      GLOB
      _xxHash_dll
      RELATIVE "${xxHash_DLL_DIR}"
      "${xxHash_DLL_DIR}/xxHash*.dll"
    )
    set(xxHash_DLL
        ${_xxHash_dll}
        # We're storing filenames only. Should we use STRING instead?
        CACHE FILEPATH
              "xxHash DLL file name"
    )
    file(
      GLOB
      _xxHash_pdb
      RELATIVE "${xxHash_DLL_DIR}"
      "${xxHash_DLL_DIR}/xxHash*.pdb"
    )
    set(xxHash_PDB
        ${_xxHash_pdb}
        CACHE FILEPATH
              "xxHash PDB file name"
    )
    mark_as_advanced(
      xxHash_DLL_DIR
      xxHash_DLL
      xxHash_PDB
    )
  endif()
else()
  set(xxHash_INCLUDE_DIRS)
  set(xxHash_LIBRARIES)
endif()

mark_as_advanced(
  xxHash_LIBRARIES
  xxHash_INCLUDE_DIRS
)
add_library(
  xxHash::xxhash
  SHARED
  IMPORTED
)
set_property(
  TARGET xxHash::xxhash
  PROPERTY IMPORTED_LOCATION
           ${xxHash_LIBRARY}
)
set_property(
  TARGET xxHash::xxhash
  APPEND
  PROPERTY INCLUDE_DIRECTORIES
           ${xxHash_INCLUDE_DIR}
)
