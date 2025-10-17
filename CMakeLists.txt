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
add_library(ssdsim SHARED api/ssdsim_api.cpp)
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

# Demo target
set(DEMO_CONFIG ${CMAKE_SOURCE_DIR}/config/default.json)
add_custom_target(run_fio_demo
  COMMAND ${fio_SOURCE_DIR}/fio
    --ioengine=external:$<TARGET_FILE:ssdsim_engine>
    --filename=${DEMO_CONFIG}
    --name=demo
    --rw=randwrite
    --size=64M
    --bs=4k
    --iodepth=8
    --numjobs=1
    --time_based
    --runtime=5
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running fio demo with simulated SSD"
)
add_dependencies(run_fio_demo ssdsim ssdsim_engine)
