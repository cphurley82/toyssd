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
project(toyssd VERSION 0.1.0 LANGUAGES CXX C)

# Global language/tooling settings
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# CMake >= 3.30 removed compatibility with projects declaring cmake_minimum_required < 3.5.
# Some third-party deps (e.g., SystemC 2.3.3) still do that. This variable is a harmless cache hint
# and a no-op on older CMake; it exists here to document intent and future-proof if honored.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Min policy version for subprojects")

include(FetchContent)
include(GNUInstallDirs) # install path helpers
include(CTest)          # provides BUILD_TESTING option

# Optional: clang-format integration
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17)
if(CLANG_FORMAT_EXE)
  message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")
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
  list(FILTER TOYSSD_FORMAT_SOURCES EXCLUDE REGEX ".*/build[^/]*/.*")
  list(FILTER TOYSSD_FORMAT_SOURCES EXCLUDE REGEX ".*/_deps/.*")

  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${TOYSSD_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting C/C++ sources with clang-format"
  )

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

# Build toggles (all default ON to preserve current behavior)
option(TOYSSD_BUILD_FIO_ENGINE "Build bundled fio external ioengine" ON)
option(TOYSSD_BUILD_DEMOS "Add demo custom targets that run fio with the engine" ON)
# BUILD_TESTING comes from CTest; keep ON by default but gate our tests under it.

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
# Disable building examples/regression in the subproject cleanly.
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

############################
# Subprojects / Targets    #
############################

# Simulator implementation library (from sim/)
add_subdirectory(sim)

# Core simulator shared library
add_library(ssdsim SHARED api/ssdsim_api.cpp api/sc_main_stub.cpp)
target_include_directories(ssdsim PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/toyssd>
)
target_link_libraries(ssdsim PRIVATE simlib systemc)
set_target_properties(ssdsim PROPERTIES EXPORT_NAME ssdsim)

# Provide a standalone sc_main stub library (useful for LD_PRELOAD on Linux)
add_library(scmain_stub SHARED api/sc_main_stub.cpp)
set_target_properties(scmain_stub PROPERTIES OUTPUT_NAME scmain_stub)

# Ensure main targets trigger formatting before building (only if targets exist)
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
  add_subdirectory(tests)
  add_test(NAME UnitTests COMMAND unit_tests)
endif()

########################################
# fio external ioengine + demos (opt)  #
########################################
if(TOYSSD_BUILD_FIO_ENGINE)
  # Fetch fio headers needed to build the external ioengine
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
  if(TOYSSD_BUILD_DEMOS)
    add_custom_target(run_fio_demo_short
      COMMAND
        ${CMAKE_COMMAND} -E env
          SSD_SIM_LIB_PATH=$<TARGET_FILE:ssdsim>
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

    # Optional: add a very short demo as a CTest (disabled by default)
    # Enable this to register a 1s, 4M smoke test named 'FioDemoShort'.
    # Run it with: ctest -R FioDemoShort --output-on-failure
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
include(CMakePackageConfigHelpers)

# Install headers (C API surface)
install(FILES api/ssdsim_api.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/toyssd)

# Install library and export targets
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
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sim/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/toyssd/sim
  FILES_MATCHING PATTERN "*.h")
