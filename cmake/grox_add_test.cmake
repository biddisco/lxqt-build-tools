# ------------------------------------------------------------------------------
function(grox_add_test category name)
  set(options FAILURE_EXPECTED RUN_SERIAL TESTING PERFORMANCE_TESTING VALGRIND)
  set(one_value_args EXECUTABLE RANKS THREADS TIMEOUT RUNWRAPPER)
  set(multi_value_args ARGS)
  cmake_parse_arguments(${name} "${options}" "${one_value_args}"
                        "${multi_value_args}" ${ARGN})

  if(NOT ${name}_RANKS)
    set(${name}_RANKS 1)
  endif()

  if(NOT ${name}_THREADS)
    set(${name}_THREADS 1)
  elseif(PIKA_WITH_TESTS_MAX_THREADS GREATER 0 AND ${name}_THREADS GREATER
                                                   PIKA_WITH_TESTS_MAX_THREADS)
    set(${name}_THREADS ${PIKA_WITH_TESTS_MAX_THREADS})
  endif()

  if(NOT ${name}_EXECUTABLE)
    set(${name}_EXECUTABLE ${name})
  endif()

  if(TARGET ${${name}_EXECUTABLE}_test)
    set(_exe "$<TARGET_FILE:${${name}_EXECUTABLE}_test>")
  elseif(TARGET ${${name}_EXECUTABLE})
    set(_exe "$<TARGET_FILE:${${name}_EXECUTABLE}>")
  else()
    set(_exe "${${name}_EXECUTABLE}")
  endif()

  if(${name}_RUN_SERIAL)
    set(run_serial TRUE)
  endif()

  # If --pika:threads=cores or all
  if(${name}_THREADS LESS_EQUAL 0)
    set(run_serial TRUE)
    if(${name}_THREADS EQUAL -1)
      set(${name}_THREADS "all")
    elseif(${name}_THREADS EQUAL -2)
      set(${name}_THREADS "cores")
    endif()
  endif()

  set(args "--pika:threads=${${name}_THREADS}")
  if(PIKA_WITH_TESTS_DEBUG_LOG)
    set(args ${args}
             "--pika:debug-pika-log=${PIKA_WITH_TESTS_DEBUG_LOG_DESTINATION}")
  endif()

  if(PIKA_WITH_PARALLEL_TESTS_BIND_NONE
     AND NOT run_serial
     AND NOT "${name}_RUNWRAPPER")
    set(args ${args} "--pika:bind=none")
  endif()

  set(args "${${name}_ARGS}" "${${name}_UNPARSED_ARGUMENTS}" ${args})

  set(_script_location ${PROJECT_BINARY_DIR})

  set(cmd ${_exe})

  if(${name}_RUNWRAPPER)
    set(_preflags_list_ ${MPIEXEC_PREFLAGS})
    separate_arguments(_preflags_list_)
    list(PREPEND cmd "${MPIEXEC_EXECUTABLE}" "${MPIEXEC_NUMPROC_FLAG}"
         "${${name}_RANKS}" ${_preflags_list_})
  endif()

  if(PIKA_WITH_TESTS_VALGRIND)
    set(valgrind_cmd ${VALGRIND_EXECUTABLE} ${PIKA_WITH_TESTS_VALGRIND_OPTIONS})
  endif()

  set(_full_name "${category}.${name}")
  add_test(NAME "${category}.${name}" COMMAND ${valgrind_cmd} ${cmd} ${args})
  if(${run_serial})
    set_tests_properties("${_full_name}" PROPERTIES RUN_SERIAL TRUE)
  endif()
  if(${name}_TIMEOUT)
    set_tests_properties("${_full_name}" PROPERTIES TIMEOUT ${${name}_TIMEOUT})
  endif()
  if(${name}_FAILURE_EXPECTED)
    set_tests_properties("${_full_name}" PROPERTIES WILL_FAIL TRUE)
  endif()
  if(${${name}_VALGRIND})
    set_tests_properties(${_full_name} PROPERTIES LABELS "VALGRIND")
  endif()

  # Only real tests, i.e. executables ending in _test, link to grox_testing
  if(TARGET ${${name}_EXECUTABLE}_test AND ${name}_TESTING)
    target_link_libraries(${${name}_EXECUTABLE}_test PRIVATE grox_testing)
  endif()

  if(TARGET ${${name}_EXECUTABLE}_test AND ${name}_PERFORMANCE_TESTING)
    target_link_libraries(${${name}_EXECUTABLE}_test
                          PRIVATE grox_performance_testing)
  endif()

endfunction(grox_add_test)

# ------------------------------------------------------------------------------
function(grox_add_test_target_dependencies category name)
  set(one_value_args PSEUDO_DEPS_NAME)
  cmake_parse_arguments(${name} "${options}" "${one_value_args}"
                        "${multi_value_args}" ${ARGN})

  # Add a custom target for this example
  pika_add_pseudo_target(${category}.${name})
  # Make pseudo-targets depend on master pseudo-target
  pika_add_pseudo_dependencies(${category} ${category}.${name})
  # Add dependencies to pseudo-target
  if(${name}_PSEUDO_DEPS_NAME)
    # When the test depend on another executable name
    pika_add_pseudo_dependencies(${category}.${name}
                                 ${${name}_PSEUDO_DEPS_NAME}${_ext})
  else()
    pika_add_pseudo_dependencies(${category}.${name} ${name}${_ext})
  endif()
endfunction(grox_add_test_target_dependencies)

# ------------------------------------------------------------------------------
# To add test to the category root as in tests/regressions/ with correct name
function(grox_add_test_and_deps category name)
  grox_add_test(tests.${category} ${name} ${ARGN})
  grox_add_test_target_dependencies(tests.${category} ${name} ${ARGN})
endfunction(grox_add_test_and_deps)
