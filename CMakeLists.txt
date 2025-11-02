cmake_minimum_required(VERSION 3.28) # Ubuntu 24.04 LTS provides CMake 3.28.

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

#
# toyssd — Top-level CMake
#
# Goals:
#  - Be explicit and well-commented for contributors.
#  - Keep defaults developer-friendly, but allow opting out of extras.
#  - Provide basic install + package config so consumers can `find_package(toyssd)`.
#  - Avoid pulling upstream subproject tests (e.g., SystemC examples).
#
# This repo builds a SystemC-based SSD simulator, an
# external fio ioengine module, and unit tests. We default to a developer-
# friendly setup and also provide basic install + package config so other
# projects ("consumers") can embed toyssd in other builds via CMake.
project(toyssd VERSION 0.1.0 LANGUAGES CXX C)

############################
# Build options (toggles)  #
############################

# These can be overridden at configure time.
# e.g. -DBUILD_TESTING=OFF -DTOYSSD_BUILD_DEMOS=OFF
option(BUILD_TESTING "Enable building unit tests" ON)
option(TOYSSD_BUILD_FIO_ENGINE "Build bundled fio external ioengine" ON)
option(TOYSSD_BUILD_DEMOS "Add demo custom targets that run fio with the engine" ON)
option(BUILD_BUNDLED_FIO "Build bundled fio from fetched sources if no system fio is found" ON)
option(TOYSSD_BUILD_PYTHON "Build Python bindings and vendored dependencies" ON)

# Tooling/quality toggles
option(TOYSSD_ENABLE_WERROR "Treat warnings as errors on toyssd targets" ON)
option(TOYSSD_ENABLE_CLANG_TIDY "Enable clang-tidy on toyssd (project) targets" ON)

# Code coverage (local + CI)
option(ENABLE_CODE_COVERAGE "Enable code coverage instrumentation and add a 'coverage' target" OFF)
set(CODE_COVERAGE_THRESHOLD 20 CACHE STRING "Fail if line coverage is below this percent when running the 'coverage' target")

# Encourage out-of-source builds to keep generated files out of the source tree.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(WARNING "In-source build detected. It's recommended to use an out-of-source build:\n  cmake -S . -B build && cmake --build build")
endif()

####################################
# Global language/tooling settings #
####################################

# Default to C++20, require at least that, but allow users to override to newer
# via -DCMAKE_CXX_STANDARD=23, etc.
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard")

# Disable GNU extensions for a clean dialect.
set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Disable compiler-specific C++ extensions")

# Position-independent code is for building shared libraries.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Developer quality-of-life: emit compile_commands.json for IDEs and tooling
# (clangd, vscode-cpptools, etc).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)

# If coverage is enabled, set appropriate compile/link flags early so all
# targets inherit instrumentation.
if(ENABLE_CODE_COVERAGE)
  message(STATUS "Code coverage instrumentation is ENABLED")
  # Favor debug-friendly builds for accurate coverage
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Use GCC-style coverage flags so gcovr can read data via llvm-cov gcov
    add_compile_options(-O0 -g -fprofile-arcs -ftest-coverage)
    add_link_options(-fprofile-arcs -ftest-coverage)
  else()
    message(WARNING "Coverage enabled, but compiler '${CMAKE_CXX_COMPILER_ID}' is not explicitly supported. Proceeding without special flags.")
  endif()
endif()

##############################
# CMake modules & utilities  #
##############################

# Tool for fetching external dependencies.
include(FetchContent)

# Provides variables in install() calls and package config paths.
include(GNUInstallDirs)

# Respects BUILD_TESTING set above.
include(CTest)

########################################
# Helper functions for target policies  #
########################################

function(toyssd_apply_werror tgt)
  if(TOYSSD_ENABLE_WERROR AND TARGET ${tgt})
    if(MSVC)
      target_compile_options(${tgt} PRIVATE /WX)
    else()
      target_compile_options(${tgt} PRIVATE -Werror)
    endif()
  endif()
endfunction()

## clang-tidy support (scoped per-target)
# Do not set CMAKE_CXX_CLANG_TIDY globally to avoid analyzing external deps.
find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17)

function(toyssd_apply_clang_tidy tgt)
  if(TOYSSD_ENABLE_CLANG_TIDY AND TARGET ${tgt} AND CLANG_TIDY_EXE)
    # Build the clang-tidy command with platform-specific include hints on macOS
    set(_CLANG_TIDY_CMD ${CLANG_TIDY_EXE})
    # Provide safe fallbacks for feature-detection builtins during analysis
    list(APPEND _CLANG_TIDY_CMD "-extra-arg=-D__has_feature(x)=0")
    if(APPLE)
      # Help clang-tidy find the SDK headers and libc++ when using Apple toolchain
      execute_process(COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE _OSX_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
      if(_OSX_SDK_PATH)
        list(APPEND _CLANG_TIDY_CMD -extra-arg=-isysroot -extra-arg=${_OSX_SDK_PATH} -extra-arg=-stdlib=libc++)
      endif()
    endif()
    set_target_properties(${tgt} PROPERTIES CXX_CLANG_TIDY "${_CLANG_TIDY_CMD}")
  endif()
endfunction()

############################
# clang-format integration #
############################

# Find clang-format executable
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17)

# Define the canonical list of project sources/headers once for reuse
file(GLOB_RECURSE TOYSSD_FORMAT_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/api/*.[ch]pp
  ${CMAKE_SOURCE_DIR}/api/*.[ch]
  ${CMAKE_SOURCE_DIR}/sim/*.[ch]pp
  ${CMAKE_SOURCE_DIR}/sim/*.[ch]
  ${CMAKE_SOURCE_DIR}/tests/*.[ch]pp
  ${CMAKE_SOURCE_DIR}/tests/*.[ch]
  ${CMAKE_SOURCE_DIR}/fio_plugin/*.[ch]
)
string(REPLACE ";" "\n" TOYSSD_FORMAT_SOURCES_NL "${TOYSSD_FORMAT_SOURCES}")
set(TOYSSD_SCRIPT_DIR ${CMAKE_BINARY_DIR}/cmake-scripts)
file(MAKE_DIRECTORY ${TOYSSD_SCRIPT_DIR})

if(CLANG_FORMAT_EXE)
  message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")

  # Helper scripts for printing file lists and invoking clang-format
  set(_CF_INPLACE_SCRIPT ${TOYSSD_SCRIPT_DIR}/run_clang_format_inplace.cmake)
  file(WRITE ${_CF_INPLACE_SCRIPT}
"message(\"clang-format will format the following files:\\n${TOYSSD_FORMAT_SOURCES_NL}\")\n"
"execute_process(COMMAND \"${CLANG_FORMAT_EXE}\" -i ${TOYSSD_FORMAT_SOURCES} WORKING_DIRECTORY \"${CMAKE_SOURCE_DIR}\" RESULT_VARIABLE rv)\n"
"if(NOT rv EQUAL 0)\n  message(FATAL_ERROR \"clang-format (in-place) failed\")\nendif()\n")

  set(_CF_CHECK_SCRIPT ${TOYSSD_SCRIPT_DIR}/run_clang_format_check.cmake)
  file(WRITE ${_CF_CHECK_SCRIPT}
"message(\"clang-format will check the following files:\\n${TOYSSD_FORMAT_SOURCES_NL}\")\n"
"execute_process(COMMAND \"${CLANG_FORMAT_EXE}\" --dry-run --Werror ${TOYSSD_FORMAT_SOURCES} WORKING_DIRECTORY \"${CMAKE_SOURCE_DIR}\" RESULT_VARIABLE rv)\n"
"if(NOT rv EQUAL 0)\n  message(FATAL_ERROR \"clang-format-check failed\")\nendif()\n")

  # Target 'format' will format all C/C++ sources in key folders.
  add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -P ${_CF_INPLACE_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting C/C++ sources with clang-format"
  )

  # Target 'format-check' will fail on formatting diffs (useful for CI).
  add_custom_target(format-check
    COMMAND ${CMAKE_COMMAND} -P ${_CF_CHECK_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking C/C++ formatting (clang-format)"
  )
else() # No CLANG_FORMAT_EXE
  # Provide no-op targets so other targets can depend on 'format' unconditionally
  add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found: skipping formatting"
  )
  add_custom_target(format-check
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found: skipping format check"
  )
endif() # if(CLANG_FORMAT_EXE)

# (format dependencies are added after targets are defined below)

#########################
# Python interpreter hint #
#########################

# If a Python virtual environment is active (e.g., via `uv` or `venv`), hint CMake
# to use that interpreter for Python-based checks (cpplint). This helps when
# configuring from a shell with the venv activated. VS Code CMake Tools does not
# automatically activate venvs; you may need to export VIRTUAL_ENV + PATH in
# cmake.configureEnvironment for the extension.
if(DEFINED ENV{VIRTUAL_ENV})
  set(_VENV_PY "$ENV{VIRTUAL_ENV}/bin/python")
  if(EXISTS "${_VENV_PY}")
    set(Python3_EXECUTABLE "${_VENV_PY}" CACHE FILEPATH "Python from active venv" FORCE)
  endif()
endif()

#########################
# cpplint (style checks) #
#########################

# Register a CTest that runs cpplint over the same files used by clang-format.
# Enabled only when BUILD_TESTING=ON and cpplint is importable via Python.
if(BUILD_TESTING)
  # Discover a Python interpreter
  find_package(Python3 COMPONENTS Interpreter QUIET)

  if(Python3_Interpreter_FOUND)
    # Check if cpplint can be imported
    execute_process(
      COMMAND ${Python3_EXECUTABLE} -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('cpplint') else 1)"
      RESULT_VARIABLE CPPLINT_IMPORT_RESULT
      OUTPUT_QUIET ERROR_QUIET
    )

    if(CPPLINT_IMPORT_RESULT EQUAL 0)
      set(_LINT_SCRIPT ${TOYSSD_SCRIPT_DIR}/run_cpplint.cmake)
      file(WRITE ${_LINT_SCRIPT}
"message(\"[cpplint files]\\n${TOYSSD_FORMAT_SOURCES_NL}\")\n"
"execute_process(COMMAND \"${Python3_EXECUTABLE}\" -m cpplint --quiet ${TOYSSD_FORMAT_SOURCES} RESULT_VARIABLE rv WORKING_DIRECTORY \"${CMAKE_SOURCE_DIR}\")\n"
"if(NOT rv EQUAL 0)\n  message(FATAL_ERROR \"cpplint failed\")\nendif()\n")
      add_test(NAME cpplint
               COMMAND ${CMAKE_COMMAND} -P ${_LINT_SCRIPT}
               WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
      set_tests_properties(cpplint PROPERTIES LABELS "lint;style")
      # Convenience custom target to run cpplint via CTest
      add_custom_target(cpplint
        COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> -R "^cpplint$" --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Run cpplint (CTest)"
      )
    else()
      message(STATUS "cpplint Python module not found; 'cpplint' CTest will not be added. Install via 'uv sync --all-extras'.")
    endif()
  else()
    message(STATUS "Python3 interpreter not found; skipping 'cpplint' CTest.")
  endif()
endif()

#########################
# clang-format (CTest)   #
#########################

# In addition to build targets, expose a CTest for format check so CI or wrappers
# can run it uniformly via ctest.
if(BUILD_TESTING AND CLANG_FORMAT_EXE)
  add_test(NAME clang-format-check
           COMMAND ${CMAKE_COMMAND} -P ${_CF_CHECK_SCRIPT}
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(clang-format-check PROPERTIES LABELS "lint;format")
endif()

#########################
# clang-tidy (custom)    #
#########################

# Provide an explicit custom target to run clang-tidy over project sources,
# independent of whether files are rebuilt. This ensures users can always
# invoke analysis and see diagnostics even when the build is up-to-date.
# Scope: api/, sim/, fio_plugin/ (tests and external deps excluded).
file(GLOB_RECURSE TOYSSD_TIDY_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/api/*.c
  ${CMAKE_SOURCE_DIR}/api/*.cc
  ${CMAKE_SOURCE_DIR}/api/*.cxx
  ${CMAKE_SOURCE_DIR}/api/*.cpp
  ${CMAKE_SOURCE_DIR}/sim/*.c
  ${CMAKE_SOURCE_DIR}/sim/*.cc
  ${CMAKE_SOURCE_DIR}/sim/*.cxx
  ${CMAKE_SOURCE_DIR}/sim/*.cpp
  ${CMAKE_SOURCE_DIR}/fio_plugin/*.c
  ${CMAKE_SOURCE_DIR}/fio_plugin/*.cc
  ${CMAKE_SOURCE_DIR}/fio_plugin/*.cxx
  ${CMAKE_SOURCE_DIR}/fio_plugin/*.cpp
)
# Exclude any paths under a tests/ subdirectory
list(FILTER TOYSSD_TIDY_SOURCES EXCLUDE REGEX ".*/tests/.*")
string(REPLACE ";" "\n" TOYSSD_TIDY_SOURCES_NL "${TOYSSD_TIDY_SOURCES}")

if(CLANG_TIDY_EXE)
  # On macOS, clang-tidy may need explicit SDK and libc++ to locate C++ headers.
  # Detect the SDK path via xcrun and pass it as extra compiler args.
  set(_TIDY_COMPILER_ARGS "")
  if(APPLE)
    execute_process(COMMAND xcrun --show-sdk-path
      OUTPUT_VARIABLE _OSX_SDK_PATH
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    if(_OSX_SDK_PATH)
      set(_TIDY_COMPILER_ARGS "-isysroot ${_OSX_SDK_PATH} -stdlib=libc++")
    endif()
  endif()
  set(_TIDY_SCRIPT ${TOYSSD_SCRIPT_DIR}/run_clang_tidy_all.cmake)
  # Placeholders for variables used by the generated script
  set(_VAR_F "\\${f}")
  set(_VAR_RV "\\${rv}")
  file(WRITE ${_TIDY_SCRIPT}
"# Auto-generated by CMake: run clang-tidy on project sources\n"
"set(TIDY_FILES\n"
)
  foreach(_f IN LISTS TOYSSD_TIDY_SOURCES)
    # Quote each file path for the script list
    file(APPEND ${_TIDY_SCRIPT} "  \"${_f}\"\n")
  endforeach()
  file(APPEND ${_TIDY_SCRIPT} ")\n")
  file(APPEND ${_TIDY_SCRIPT}
"message(\"clang-tidy will analyze the following files:\n${TOYSSD_TIDY_SOURCES_NL}\")\n"
"set(failed 0)\n"
"foreach(f IN LISTS TIDY_FILES)\n"
"  execute_process(COMMAND \"${CLANG_TIDY_EXE}\" \"${_VAR_F}\" -- -std=c++20 -I\"${CMAKE_SOURCE_DIR}\" -I\"${CMAKE_SOURCE_DIR}/api\" -I\"${CMAKE_SOURCE_DIR}/sim\" -I\"${CMAKE_BINARY_DIR}/_deps/systemc-src/src\" ${_TIDY_COMPILER_ARGS} RESULT_VARIABLE rv)\n"
"  if(NOT rv EQUAL 0)\n    message(WARNING \"clang-tidy returned non-zero for: ${_VAR_F} (exit ${_VAR_RV})\")\n    set(failed 1)\n  endif()\n"
"endforeach()\n"
"if(failed)\n  message(WARNING \"clang-tidy reported issues (non-fatal).\")\nendif()\n")

  add_custom_target(clang-tidy
    COMMAND ${CMAKE_COMMAND} -P ${_TIDY_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy on project sources"
  )
else()
  add_custom_target(clang-tidy
    COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found: skipping analysis"
  )
endif()

############################
# External Dependencies    #
############################

# GoogleTest (only when testing is enabled)
if(BUILD_TESTING)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
  )
  set(INSTALL_GTEST OFF)
  FetchContent_MakeAvailable(googletest)
endif() # 

# SystemC (3.0.2) — build from source via ExternalProject
include(ExternalProject)

set(SYSTEMC_VERSION 3.0.2)
set(SYSTEMC_PREFIX ${CMAKE_BINARY_DIR}/_deps/systemc)
file(MAKE_DIRECTORY ${SYSTEMC_PREFIX}/include)
file(MAKE_DIRECTORY ${SYSTEMC_PREFIX}/lib)

ExternalProject_Add(systemc_ext
  URL https://github.com/accellera-official/systemc/archive/refs/tags/${SYSTEMC_VERSION}.tar.gz
  UPDATE_DISCONNECTED 1
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${SYSTEMC_PREFIX}
    -DCMAKE_BUILD_TYPE=$<IF:$<CONFIG:>,${CMAKE_BUILD_TYPE},$<CONFIG>>
    -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
    -DENABLE_EXAMPLES=OFF
    -DENABLE_REGRESSION=OFF
  BUILD_BYPRODUCTS
    ${SYSTEMC_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}systemc${CMAKE_SHARED_LIBRARY_SUFFIX}
)

add_library(systemc SHARED IMPORTED)
add_dependencies(systemc systemc_ext)

if(APPLE)
  set(SYSTEMC_LIB ${SYSTEMC_PREFIX}/lib/libsystemc.dylib)
elseif(WIN32)
  set(SYSTEMC_LIB ${SYSTEMC_PREFIX}/lib/systemc.lib)
else()
  set(SYSTEMC_LIB ${SYSTEMC_PREFIX}/lib/libsystemc.so)
endif()

set_target_properties(systemc PROPERTIES
  IMPORTED_LOCATION ${SYSTEMC_LIB}
  INTERFACE_INCLUDE_DIRECTORIES ${SYSTEMC_PREFIX}/include
)

# PySysC (vendored from fork with Python 3.9+ fix)
FetchContent_Declare(
  pysysc_src
  GIT_REPOSITORY https://github.com/cphurley82/PySysC.git
  GIT_TAG fix-pyeval-init
)
FetchContent_GetProperties(pysysc_src)
if(NOT pysysc_src_POPULATED)
  FetchContent_Populate(pysysc_src)
endif()
set(PYSYSC_SOURCE_DIR ${pysysc_src_SOURCE_DIR})

if(TOYSSD_BUILD_PYTHON)
  set(_pysysc_module ${PYSYSC_SOURCE_DIR}/PyScModule.cpp)
  if(EXISTS ${_pysysc_module})
    file(READ ${_pysysc_module} _pysysc_contents)
    string(REPLACE "#if PY_VERSION_HEX < 0x03090000\n    if (!PyEval_ThreadsInitialized())\n        PyEval_InitThreads();\n#else\n    PyEval_InitThreads();\n#endif\n"
                   "#if PY_VERSION_HEX < 0x03090000\n    if (!PyEval_ThreadsInitialized())\n        PyEval_InitThreads();\n#endif\n"
                   _pysysc_patched "${_pysysc_contents}")
    if(NOT "${_pysysc_contents}" STREQUAL "${_pysysc_patched}")
      file(WRITE ${_pysysc_module} "${_pysysc_patched}")
    endif()
  endif()
endif()

if(TOYSSD_BUILD_PYTHON)
  find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)

  Python3_add_library(pysyscsc MODULE
    ${PYSYSC_SOURCE_DIR}/PyScModule.cpp
  )
  target_compile_features(pysyscsc PRIVATE cxx_std_17)
  target_include_directories(pysyscsc PRIVATE ${PYSYSC_SOURCE_DIR})
  target_compile_definitions(pysyscsc PRIVATE MAJOR_VERSION=1 MINOR_VERSION=0)
  target_link_libraries(pysyscsc PRIVATE Python3::Module systemc)
  add_dependencies(pysyscsc systemc_ext)
  if(APPLE)
    set(_pysysc_rpath "@loader_path;@loader_path/../_vendor/systemc/lib")
  else()
    set(_pysysc_rpath "$ORIGIN;$ORIGIN/../_vendor/systemc/lib")
  endif()
  set_target_properties(pysyscsc PROPERTIES
    OUTPUT_NAME "pysyscsc"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/python"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/python"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/python"
    INSTALL_RPATH "${_pysysc_rpath}"
    BUILD_RPATH "${_pysysc_rpath}"
  )
  toyssd_apply_werror(pysyscsc)
  toyssd_apply_clang_tidy(pysyscsc)
endif()

############################
# Subprojects / Targets    #
############################

# Simulator implementation library (from sim/)
# Contains most of the simulator code (SystemC components, firmware/FTL, etc.).
add_subdirectory(sim)

# Core simulator shared library
# Provides a C API surface (api/ssdsim_api.h, api/ssdsim_api.cpp) that can be
# loaded by the fio engine. Also includes a tiny sc_main stub to satisfy
# linkage for consumers that need it.
add_library(ssdsim SHARED api/ssdsim_api.cpp api/sc_main_stub.cpp)
target_compile_features(ssdsim PUBLIC cxx_std_20)
target_include_directories(ssdsim PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/toyssd>
)
target_link_libraries(ssdsim PRIVATE simlib systemc)
set_target_properties(ssdsim PROPERTIES EXPORT_NAME ssdsim)
add_dependencies(ssdsim format)
toyssd_apply_werror(ssdsim)
toyssd_apply_clang_tidy(ssdsim)

# Provide a standalone sc_main stub library.
# Useful with LD_PRELOAD on Linux to intercept/override sc_main resolution.
add_library(scmain_stub SHARED api/sc_main_stub.cpp)
target_compile_features(scmain_stub PUBLIC cxx_std_20)
set_target_properties(scmain_stub PROPERTIES 
  OUTPUT_NAME scmain_stub
  EXPORT_NAME scmain_stub
)
add_dependencies(scmain_stub format)
toyssd_apply_werror(scmain_stub)
toyssd_apply_clang_tidy(scmain_stub)

########################################
# Tests (only if BUILD_TESTING is ON)  #
########################################
if(BUILD_TESTING)
  # Enable unit tests (registered in tests/ via gtest_discover_tests).
  add_subdirectory(tests)
  # Tips: ctest -R <regex> --output-on-failure. GoogleTest XML is written to
  # build/test-results/gtest/ for CI ingestion.
endif()

########################################
# fio external ioengine + demos (opt)  #
########################################

# The fio external ioengine is a MODULE that is dlopen()'d by fio at runtime.
# We only need fio headers during compilation of the engine itself. At runtime,
# we try to use a system 'fio' if present; otherwise we can build a bundled one
# from fetched sources when BUILD_BUNDLED_FIO=ON. The demo targets below honor
# these choices and will depend on the bundled build when needed.
if(TOYSSD_BUILD_FIO_ENGINE)
  # Fetch fio headers needed to build the external ioengine
  FetchContent_Declare(
    fio
    GIT_REPOSITORY https://github.com/axboe/fio.git
    GIT_TAG fio-3.41
  )
  FetchContent_MakeAvailable(fio)
  set(FIO_INCLUDE_DIR ${fio_SOURCE_DIR})

  # Build fio external ioengine module that will dlopen libssdsim at runtime
  add_library(ssdsim_engine MODULE fio_plugin/ssdsim_engine.c)
  # Treat fio headers as system includes so warnings in dependency headers
  # do not trip -Werror on our local target.
  target_include_directories(ssdsim_engine SYSTEM PRIVATE ${FIO_INCLUDE_DIR})

  # macOS-specific tweaks:
  if(APPLE)
    # Avoid conflicts with fortified headers and legacy typedefs in fio os-mac.h
    target_compile_definitions(ssdsim_engine PRIVATE CONFIG_CLOCKID_T CONFIG_STRLCAT)

    # Allow undefined symbols to be resolved at runtime by the fio binary (Darwin)
    target_link_options(ssdsim_engine PRIVATE -Wl,-undefined,dynamic_lookup)
  endif()

  # Demo configuration options
  set(DEMO_CONFIG    ${CMAKE_SOURCE_DIR}/config/default.json CACHE STRING "Path to JSON config for demo")
  set(DEMO_RW        randwrite                         CACHE STRING "fio rw mode (read, write, randread, randwrite, etc.)")
  set(DEMO_SIZE      64M                               CACHE STRING "Total IO size for the demo")
  set(DEMO_BS        4k                                CACHE STRING "Block size for the demo")
  set(DEMO_IODEPTH   8                                 CACHE STRING "iodepth for the demo")
  set(DEMO_NUMJOBS   1                                 CACHE STRING "numjobs for the demo")
  set(DEMO_RUNTIME_S 1                                 CACHE STRING "Runtime (seconds) for the demo when time_based is used")

  # Allow overriding fio executable path if desired
  set(FIO_EXE_OVERRIDE "" CACHE FILEPATH "Override path to fio binary; if empty, prefer bundled fio for reproducibility")
  option(BUILD_BUNDLED_FIO "Build and prefer bundled fio from fetched sources (recommended for consistent demos/tests)" ON)

  # Selection logic: prefer bundled fio unless explicitly overridden
  set(USE_BUNDLED_FIO OFF)
  set(HAVE_SYSTEM_FIO OFF)
  if(FIO_EXE_OVERRIDE)
    set(FIO_EXE ${FIO_EXE_OVERRIDE})
  elseif(BUILD_BUNDLED_FIO)
    set(USE_BUNDLED_FIO ON)
    set(FIO_EXE ${fio_SOURCE_DIR}/fio)
    # Build fio using its Makefile in the source tree.
    add_custom_target(fio_bundled ALL
      COMMAND ${CMAKE_COMMAND} -E env PATH=$ENV{PATH} make -j
      WORKING_DIRECTORY ${fio_SOURCE_DIR}
      COMMENT "Building bundled fio from fetched sources")
  else()
    # Fall back to a system-installed fio if bundling is disabled
    find_program(FIO_EXE NAMES fio)
    if(FIO_EXE)
      set(HAVE_SYSTEM_FIO ON)
    else()
      message(STATUS "No system 'fio' found and BUILD_BUNDLED_FIO=OFF. You may need to set FIO_EXE_OVERRIDE or install fio.")
    endif()
  endif()

  # Warn only when we neither use bundled fio nor have a resolvable executable
  if(NOT USE_BUNDLED_FIO AND NOT FIO_EXE_OVERRIDE AND NOT HAVE_SYSTEM_FIO)
    message(WARNING "fio executable not found. The demo may fail unless fio is available in PATH or bundled fio is enabled.")
  endif()

  # Register the demo as a CTest so automation can run it via `ctest`.
  if(TOYSSD_BUILD_DEMOS)
    if(APPLE)
      add_test(NAME FioDemo
        COMMAND
          ${CMAKE_COMMAND} -E env
            SSD_SIM_LIB_PATH=$<TARGET_FILE:ssdsim>
            DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:ssdsim>:$<TARGET_FILE_DIR:systemc>
            ${FIO_EXE}
          --ioengine=external:$<TARGET_FILE:ssdsim_engine>
          --filename=${DEMO_CONFIG}
          --name=demo
          --rw=${DEMO_RW}
          --size=${DEMO_SIZE}
          --bs=${DEMO_BS}
          --iodepth=${DEMO_IODEPTH}
          --numjobs=${DEMO_NUMJOBS}
          --time_based
          --runtime=${DEMO_RUNTIME_S}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      )
    else()
      add_test(NAME FioDemo
        COMMAND
          ${CMAKE_COMMAND} -E env
            SSD_SIM_LIB_PATH=$<TARGET_FILE:ssdsim>
            LD_LIBRARY_PATH=$<TARGET_FILE_DIR:ssdsim>:$<TARGET_FILE_DIR:systemc>
            LD_PRELOAD=$<TARGET_FILE:scmain_stub>
            ${FIO_EXE}
          --ioengine=external:$<TARGET_FILE:ssdsim_engine>
          --filename=${DEMO_CONFIG}
          --name=demo
          --rw=${DEMO_RW}
          --size=${DEMO_SIZE}
          --bs=${DEMO_BS}
          --iodepth=${DEMO_IODEPTH}
          --numjobs=${DEMO_NUMJOBS}
          --time_based
          --runtime=${DEMO_RUNTIME_S}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      )
    endif()
    set_tests_properties(FioDemo PROPERTIES LABELS "demo")
  endif()
  # Apply policies to the ioengine too
  toyssd_apply_werror(ssdsim_engine)
  toyssd_apply_clang_tidy(ssdsim_engine)
endif() # TOYSSD_BUILD_FIO_ENGINE

########################################
# Coverage target (gcovr)
########################################
if(ENABLE_CODE_COVERAGE)
  # Try to locate gcovr; required to generate reports and enforce threshold
  find_program(GCOVR_EXE NAMES gcovr)
  if(NOT GCOVR_EXE)
    message(STATUS "gcovr not found. Install via 'pip install gcovr' or 'pipx install gcovr'. A helper 'coverage' target will print instructions.")
  endif()

  # Exclude external deps, tests, and build artifacts by default.
  # Quote regexes to prevent shell glob expansion on macOS/Linux.
  set(COVERAGE_EXCLUDES
    --exclude ".*_deps/.*"
    --exclude ".*/tests/.*"
    --exclude ".*/Testing/.*"
    --exclude ".*/build.*/.*"
  )

  set(COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)
  file(MAKE_DIRECTORY ${COVERAGE_DIR})

  if(GCOVR_EXE)
    add_custom_target(coverage
      COMMAND ${CMAKE_COMMAND} -E echo "Running tests and generating coverage report - threshold ${CODE_COVERAGE_THRESHOLD}%"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -LE demo
      COMMAND ${GCOVR_EXE}
              --root ${CMAKE_SOURCE_DIR}
              --print-summary
              --xml ${COVERAGE_DIR}/coverage.xml
              --html --html-details -o ${COVERAGE_DIR}/index.html
              --fail-under-line ${CODE_COVERAGE_THRESHOLD}
              --exclude='.*_deps/.*'
              --exclude='.*/tests/.*'
              --exclude='.*/Testing/.*'
              --exclude='.*/build.*/.*'
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "gcovr: create XML and HTML reports; enforce minimum coverage"
    )
  else()
    # Provide a helper target that explains how to enable coverage tooling
    add_custom_target(coverage
      COMMAND ${CMAKE_COMMAND} -E echo "ERROR: 'gcovr' not found. Install with: python3 -m pip install --user gcovr"
      COMMAND ${CMAKE_COMMAND} -E false
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "gcovr not found"
    )
  endif()

  # Ensure unit tests are built before attempting to run ctest from the target
  if(TARGET unit_tests)
    add_dependencies(coverage unit_tests)
  endif()
  if(TARGET ssdsim)
    add_dependencies(coverage ssdsim)
  endif()
  if(TARGET simlib)
    add_dependencies(coverage simlib)
  endif()
endif()

########################################
# Install & Package (basic)            #
########################################
include(CMakePackageConfigHelpers)

# Install headers (C API surface)
# Downstream consumers can `find_package(toyssd CONFIG)` and link to targets
# exported below. We install the public C API header under include/toyssd/.
install(FILES api/ssdsim_api.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/toyssd)
install(FILES api/ssdsim_api.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/api)

# Install library and export targets
# Install shared libraries and export targets (toyssd::ssdsim, toyssd::simlib)
set(_toyssd_export_targets ssdsim simlib scmain_stub)
if(TOYSSD_BUILD_PYTHON AND TARGET toyssd_pycore)
  list(APPEND _toyssd_export_targets toyssd_pycore)
endif()

install(TARGETS ${_toyssd_export_targets}
        EXPORT toyssdTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Build-tree export for convenience
export(EXPORT toyssdTargets
       NAMESPACE toyssd::
       FILE ${CMAKE_CURRENT_BINARY_DIR}/toyssdTargets.cmake)

# Package config files
#  - toyssdConfig.cmake: includes exported targets
#  - toyssdConfigVersion.cmake: enforces SameMajorVersion compatibility
set(toyssd_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/toyssd)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/toyssdConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/toyssdConfig.cmake
  INSTALL_DESTINATION ${toyssd_INSTALL_CMAKEDIR}
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/toyssdConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
          ${CMAKE_CURRENT_BINARY_DIR}/toyssdConfig.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/toyssdConfigVersion.cmake
        DESTINATION ${toyssd_INSTALL_CMAKEDIR})

install(EXPORT toyssdTargets
        NAMESPACE toyssd::
        DESTINATION ${toyssd_INSTALL_CMAKEDIR})

# Install internal headers for advanced consumers (sim library)
# This exposes headers under include/toyssd/sim for projects that want deeper
# integration with simlib internals. Keep stable C API in api/.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sim/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/toyssd/sim
  FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sim/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sim
  FILES_MATCHING PATTERN "*.h")

if(TOYSSD_BUILD_PYTHON)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/toyssd/
    DESTINATION toyssd
    FILES_MATCHING
      PATTERN "*.py"
      PATTERN "py.typed"
  )
  if(TARGET toyssd_pycore)
    install(TARGETS toyssd_pycore scmain_stub simlib
      LIBRARY DESTINATION toyssd/_native
      RUNTIME DESTINATION toyssd/_native
      ARCHIVE DESTINATION toyssd/_native
    )
  endif()
  if(TARGET pysyscsc)
    install(TARGETS pysyscsc
      LIBRARY DESTINATION toyssd/_vendor
      RUNTIME DESTINATION toyssd/_vendor
      ARCHIVE DESTINATION toyssd/_vendor
    )
    install(DIRECTORY ${PYSYSC_SOURCE_DIR}/pysysc/
      DESTINATION toyssd/_vendor/pysysc
      FILES_MATCHING PATTERN "*.py"
    )
    if(EXISTS ${PYSYSC_SOURCE_DIR}/LICENSE)
      install(FILES ${PYSYSC_SOURCE_DIR}/LICENSE
        DESTINATION toyssd/_vendor/pysysc
        RENAME LICENSE.PySysC
      )
    endif()
  endif()
  install(DIRECTORY ${SYSTEMC_PREFIX}/include/
    DESTINATION toyssd/_vendor/systemc/include
  )
  install(DIRECTORY ${SYSTEMC_PREFIX}/lib/
    DESTINATION toyssd/_vendor/systemc/lib
    FILES_MATCHING
      PATTERN "libsystemc*${CMAKE_SHARED_LIBRARY_SUFFIX}*"
      PATTERN "systemc.lib"
  )
endif()
