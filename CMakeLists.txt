cmake_minimum_required(VERSION 3.16)

#
# toyssd — Top-level CMake
#
# Goals:
#  - Be explicit and well-commented for contributors.
#  - Keep defaults developer-friendly, but allow opting out of extras.
#  - Provide basic install + package config so consumers can `find_package(toyssd)`.
#  - Avoid pulling upstream subproject tests (e.g., SystemC examples).
#
# Root project declaration. This repo builds a SystemC-based SSD simulator, an
# external fio ioengine module, and unit tests. We default to a developer-
# friendly setup and keep consumers in mind via a basic install + package.
# TODO: Why do we need to "keep consumers in mind" who are the consumers?
project(toyssd VERSION 0.1.0 LANGUAGES CXX C)

# Global language/tooling settings
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Developer quality-of-life: emit compile_commands.json for IDEs and tooling
# (clangd, vscode-cpptools, etc). Harmless for consumers.
# TODO: Who are the consumers?
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)

# CMake >= 3.30 removed compatibility with projects declaring cmake_minimum_required < 3.5.
# Some third-party deps (e.g., SystemC 2.3.3) still do that. This variable is a harmless cache hint
# and a no-op on older CMake; it exists here to document intent and future-proof if honored.
# TODO: We switched to SystemC 3.0.1+, do we still need this?
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Min policy version for subprojects")

include(FetchContent)
include(GNUInstallDirs) # install path helpers # TODO: Why do we need this?
include(CTest)          # provides BUILD_TESTING option

############################
# clang-format integration #
############################

# Find clang-format executable
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17)

if(CLANG_FORMAT_EXE)
  message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")
  
  # Find all source files to format
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
  # Exclude anything under build or _deps just in case
  # TODO: Is this necessary when the build should be outside the source tree?
  list(FILTER TOYSSD_FORMAT_SOURCES EXCLUDE REGEX ".*/build[^/]*/.*")
  list(FILTER TOYSSD_FORMAT_SOURCES EXCLUDE REGEX ".*/_deps/.*")

  # Target 'format' will format all C/C++ sources in key folders.
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${TOYSSD_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting C/C++ sources with clang-format"
  )

  # Target 'format-check' will fail on formatting diffs (useful for CI).
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${TOYSSD_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking C/C++ formatting (clang-format)"
  )
else()
  # Provide no-op targets so other targets can depend on 'format' unconditionally
  add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found: skipping formatting"
  )
  add_custom_target(format-check
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found: skipping format check"
  )
endif()

# (format dependencies are added after targets are defined below)

############################
# External Dependencies    #
############################

# Build toggles (all default ON to preserve current behavior)
option(TOYSSD_BUILD_FIO_ENGINE "Build bundled fio external ioengine" ON)
option(TOYSSD_BUILD_DEMOS "Add demo custom targets that run fio with the engine" ON)
# BUILD_TESTING comes from CTest; keep ON by default but gate our tests under it.
# TODO: Can the above options be put at the top of the file for visibility or closer to the other fio stuff?

# GoogleTest (only when testing is enabled)
if(BUILD_TESTING)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
  )
  set(INSTALL_GTEST OFF)
  FetchContent_MakeAvailable(googletest)
endif()

# SystemC (3.0.1+)
# We fetch SystemC as a subproject. To speed up builds and keep logs clean,
# disable examples and regression tests in the upstream project.
set(ENABLE_EXAMPLES   OFF CACHE BOOL "Disable SystemC examples" FORCE)
set(ENABLE_REGRESSION OFF CACHE BOOL "Disable SystemC regression tests" FORCE)
FetchContent_Declare(
  systemc
  GIT_REPOSITORY https://github.com/accellera-official/systemc.git
  GIT_TAG 3.0.1
)
set(SYSTEMC_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(SystemC_CXX_STANDARD 20 CACHE STRING "SystemC C++ standard")
FetchContent_MakeAvailable(systemc)

# fio (headers only for external ioengine build)
# The external ioengine is a MODULE that is dlopen()'d by fio at runtime.
# We only need fio headers during compilation of the engine itself. At runtime,
# we try to use a system 'fio' if present, otherwise build a bundled one from
# fetched sources (opt-in via BUILD_BUNDLED_FIO).
# TODO: The above comment seems out of place here since fio is only fetched later.

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
target_include_directories(ssdsim PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/toyssd>
)
target_link_libraries(ssdsim PRIVATE simlib systemc)
set_target_properties(ssdsim PROPERTIES EXPORT_NAME ssdsim)

# Provide a standalone sc_main stub library.
# Useful with LD_PRELOAD on Linux to intercept/override sc_main resolution.
add_library(scmain_stub SHARED api/sc_main_stub.cpp)
set_target_properties(scmain_stub PROPERTIES OUTPUT_NAME scmain_stub)

# Ensure main targets trigger formatting before building (only if targets exist)
# TODO: Is this the simplest way to do this, could we just add the dependency unconditionally where the target is defined?
if(TARGET format)
  foreach(_t IN ITEMS simlib ssdsim scmain_stub)
    if(TARGET ${_t})
      add_dependencies(${_t} format)
    endif()
  endforeach()
endif()

########################################
# Tests (only if BUILD_TESTING is ON)  #
########################################
if(BUILD_TESTING)
  # Our tests live under tests/ and are only included when BUILD_TESTING=ON.
  # Note: tests/CMakeLists also uses gtest_discover_tests for per-test
  # registration; the add_test below is a convenience wrapper that runs the
  # whole unit_tests binary and is kept for compatibility with existing CI.
  add_subdirectory(tests)
  # TODO: This is redundant with gtest_discover_tests, probably should be removed but may need to re-work CI first to run the discovered tests.
  add_test(NAME UnitTests COMMAND unit_tests)
endif()

########################################
# fio external ioengine + demos (opt)  #
########################################
if(TOYSSD_BUILD_FIO_ENGINE)
  # Fetch fio headers needed to build the external ioengine
  # TODO: Should we switch to a newer version of fio?
  FetchContent_Declare(
    fio
    GIT_REPOSITORY https://github.com/axboe/fio.git
    GIT_TAG fio-3.36
  )
  FetchContent_MakeAvailable(fio)
  set(FIO_INCLUDE_DIR ${fio_SOURCE_DIR})
  # Build fio external ioengine module that will dlopen libssdsim at runtime
  add_library(ssdsim_engine MODULE fio_plugin/ssdsim_engine.c)
  target_include_directories(ssdsim_engine PRIVATE ${FIO_INCLUDE_DIR})
  # Avoid conflicts with macOS fortified headers and legacy typedefs in fio os-mac.h
  if(APPLE)
    target_compile_definitions(ssdsim_engine PRIVATE CONFIG_CLOCKID_T CONFIG_STRLCAT)
  endif()
  # Allow undefined symbols to be resolved at runtime by the fio binary (Darwin)
  if(APPLE)
    target_link_options(ssdsim_engine PRIVATE -Wl,-undefined,dynamic_lookup)
  endif()
  # TODO: The above two apple-specific blocks could be combined.
  set_target_properties(ssdsim_engine PROPERTIES OUTPUT_NAME ssdsim_engine)

  # Demo configuration (prefers system 'fio' if present, otherwise fetched source build)
  set(DEMO_CONFIG    ${CMAKE_SOURCE_DIR}/config/default.json CACHE STRING "Path to JSON config for demo")
  set(DEMO_RW        randwrite                         CACHE STRING "fio rw mode (read, write, randread, randwrite, etc.)")
  set(DEMO_SIZE      64M                               CACHE STRING "Total IO size for the demo")
  set(DEMO_BS        4k                                CACHE STRING "Block size for the demo")
  set(DEMO_IODEPTH   8                                 CACHE STRING "iodepth for the demo")
  set(DEMO_NUMJOBS   1                                 CACHE STRING "numjobs for the demo")
  set(DEMO_RUNTIME_S 5                                 CACHE STRING "Runtime (seconds) for the demo when time_based is used")

  # Allow overriding fio executable path if desired
  set(FIO_EXE_OVERRIDE "" CACHE FILEPATH "Override path to fio binary; if empty, auto-detect")
  option(BUILD_BUNDLED_FIO "Build bundled fio from fetched sources if no system fio is found" ON)

  set(HAVE_SYSTEM_FIO OFF)
  if(FIO_EXE_OVERRIDE)
    set(FIO_EXE ${FIO_EXE_OVERRIDE})
    set(HAVE_SYSTEM_FIO ON)
  else()
    find_program(FIO_EXE NAMES fio)
    if(FIO_EXE)
      set(HAVE_SYSTEM_FIO ON)
    else()
      # Fallback to the fetched source tree; we can build it if requested.
      set(FIO_EXE ${fio_SOURCE_DIR}/fio)
      if(BUILD_BUNDLED_FIO)
        message(STATUS "No system 'fio' found. BUILD_BUNDLED_FIO=ON: will use bundled fio at ${FIO_EXE} (built from sources).")
        # Build fio using its Makefile in the source tree.
        add_custom_target(fio_bundled
          COMMAND ${CMAKE_COMMAND} -E env PATH=$ENV{PATH} make -j
          WORKING_DIRECTORY ${fio_SOURCE_DIR}
          COMMENT "Building bundled fio from fetched sources")
      else()
        message(STATUS "No system 'fio' found and BUILD_BUNDLED_FIO=OFF. You may need to set FIO_EXE_OVERRIDE or install fio.")
      endif()
    endif()
  endif()
  
  if(NOT EXISTS ${FIO_EXE})
    message(WARNING "fio executable not found at '${FIO_EXE}'. The demo target may fail unless fio is available in PATH or bundled fio is built.")
  endif()

  if(TOYSSD_BUILD_DEMOS)
    add_custom_target(run_fio_demo
      COMMAND
        ${CMAKE_COMMAND} -E env
          SSD_SIM_LIB_PATH=$<TARGET_FILE:ssdsim>
          # Platform-specific dynamic loader variables for resolving libssdsim and libsystemc:
          #  - macOS: DYLD_LIBRARY_PATH
          #  - Linux/others: LD_LIBRARY_PATH (+ LD_PRELOAD for scmain_stub)
          $<$<PLATFORM_ID:Darwin>:DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:ssdsim>:$<TARGET_FILE_DIR:systemc>>
          $<$<NOT:$<PLATFORM_ID:Darwin>>:LD_LIBRARY_PATH=$<TARGET_FILE_DIR:ssdsim>:$<TARGET_FILE_DIR:systemc>>
          $<$<NOT:$<PLATFORM_ID:Darwin>>:LD_PRELOAD=$<TARGET_FILE:scmain_stub>>
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
      COMMENT "Running fio demo with simulated SSD"
    )
    add_dependencies(run_fio_demo ssdsim ssdsim_engine scmain_stub)
    if(NOT HAVE_SYSTEM_FIO AND BUILD_BUNDLED_FIO)
      add_dependencies(run_fio_demo fio_bundled)
    endif()
    
    # Convenience alias target
    add_custom_target(demo DEPENDS run_fio_demo)
  endif()

  # Short demo target for quick smoke (1s, 4M)
  # TODO: I don't see the point for a short demo target when we have the full
  # demo above, we could just make the above demo shorter if needed. However,
  # we do want to ensure a fio test runs with CTest.
  if(TOYSSD_BUILD_DEMOS)
    add_custom_target(run_fio_demo_short
      COMMAND
        ${CMAKE_COMMAND} -E env
          SSD_SIM_LIB_PATH=$<TARGET_FILE:ssdsim>
          # See comments above for platform-specific env vars
          $<$<PLATFORM_ID:Darwin>:DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:ssdsim>:$<TARGET_FILE_DIR:systemc>>
          $<$<NOT:$<PLATFORM_ID:Darwin>>:LD_LIBRARY_PATH=$<TARGET_FILE_DIR:ssdsim>:$<TARGET_FILE_DIR:systemc>>
          $<$<NOT:$<PLATFORM_ID:Darwin>>:LD_PRELOAD=$<TARGET_FILE:scmain_stub>>
          ${FIO_EXE}
        --ioengine=external:$<TARGET_FILE:ssdsim_engine>
        --filename=${DEMO_CONFIG}
        --name=demo_short
        --rw=${DEMO_RW}
        --size=4M
        --bs=${DEMO_BS}
        --iodepth=${DEMO_IODEPTH}
        --numjobs=1
        --time_based
        --runtime=1
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Running short fio demo (1s, 4M) with simulated SSD"
    )
    add_dependencies(run_fio_demo_short ssdsim ssdsim_engine scmain_stub)
    if(NOT HAVE_SYSTEM_FIO AND BUILD_BUNDLED_FIO)
      add_dependencies(run_fio_demo_short fio_bundled)
    endif()

    # Optional: add a very short demo as a CTest (disabled by default).
    # Enable this to register a 1s, 4M smoke test named 'FioDemoShort'.
    # Usage:
    #   cmake -DTOYSSD_DEMO_TEST=ON ...
    #   ctest -R FioDemoShort --output-on-failure
    option(TOYSSD_DEMO_TEST "Enable short ctest demo 'FioDemoShort' (1s, 4M). Run with: ctest -R FioDemoShort --output-on-failure" OFF)
    if(TOYSSD_DEMO_TEST)
      message(STATUS "TOYSSD_DEMO_TEST=ON: Registering 'FioDemoShort' (1s, 4M). Run: ctest -R FioDemoShort --output-on-failure")
      add_test(NAME FioDemoShort
               COMMAND ${CMAKE_COMMAND} --build . --target run_fio_demo_short -j1
               WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    endif()
  endif()
endif() # TOYSSD_BUILD_FIO_ENGINE

########################################
# Install & Package (basic)            #
########################################
# TODO: Why would we need to install & package anything? One day the simulator
# might be seperated from the firmware but is this added complication worth it
# now?
include(CMakePackageConfigHelpers)

# Install headers (C API surface)
# Downstream consumers can `find_package(toyssd CONFIG)` and link to targets
# exported below. We install the public C API header under include/toyssd/.
install(FILES api/ssdsim_api.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/toyssd)

# Install library and export targets
# Install shared libraries and export targets (toyssd::ssdsim, toyssd::simlib)
install(TARGETS ssdsim simlib
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
# Install internal headers for advanced consumers (sim library)
# This exposes headers under include/toyssd/sim for projects that want deeper
# integration with simlib internals. Keep stable C API in api/.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sim/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/toyssd/sim
  FILES_MATCHING PATTERN "*.h")
