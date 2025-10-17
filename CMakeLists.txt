cmake_minimum_required(VERSION 3.16)
project(toy-ssd-sim LANGUAGES CXX C)

# Force C++20 globally (in cache) so all subprojects, including SystemC
# built via FetchContent, use the same standard and link ABI.
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# CMake >= 3.30 removed compatibility with projects declaring cmake_minimum_required < 3.5.
# Some third-party deps (e.g., SystemC 2.3.3) still do that. This allows configuring them.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
include(CTest)
enable_testing()

# GoogleTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
set(INSTALL_GTEST OFF)
FetchContent_MakeAvailable(googletest)

# SystemC
# Disable testing while adding SystemC to avoid pulling in its example tests.
set(CMAKE_DISABLE_TESTING ON CACHE BOOL "Disable testing in subprojects" FORCE)
FetchContent_Declare(
  systemc
  GIT_REPOSITORY https://github.com/accellera-official/systemc.git
  GIT_TAG 2.3.3
)
set(SYSTEMC_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(SystemC_CXX_STANDARD 20 CACHE STRING "SystemC C++ standard")
FetchContent_MakeAvailable(systemc)
# Re-enable testing for this project (our tests)
set(CMAKE_DISABLE_TESTING OFF CACHE BOOL "Disable testing in subprojects" FORCE)

# fio (headers only for external ioengine build)
FetchContent_Declare(
  fio
  GIT_REPOSITORY https://github.com/axboe/fio.git
  GIT_TAG fio-3.36
)
FetchContent_MakeAvailable(fio)
set(FIO_INCLUDE_DIR ${fio_SOURCE_DIR})

# Subdirs
add_subdirectory(sim)
add_subdirectory(tests)

# Core simulator shared library
add_library(ssdsim SHARED api/ssdsim_api.cpp api/sc_main_stub.cpp)
target_include_directories(ssdsim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/api)
target_link_libraries(ssdsim PRIVATE simlib systemc)
install(TARGETS ssdsim)

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

# Tests
add_test(NAME UnitTests COMMAND unit_tests)

# Demo configuration and target (prefers system 'fio' if present, otherwise the fetched source build)
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

# Provide a standalone sc_main stub library (useful for LD_PRELOAD on Linux)
add_library(scmain_stub SHARED api/sc_main_stub.cpp)
set_target_properties(scmain_stub PROPERTIES OUTPUT_NAME scmain_stub)

# Short demo target for quick smoke (1s, 4M)
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
