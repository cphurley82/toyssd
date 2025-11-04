cmake_minimum_required(VERSION 3.24)

project(
    toyssd
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TOYSSD_BUILD_TESTS "Build toyssd unit tests" ON)
option(TOYSSD_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(
    systemc
    URL https://github.com/accellera-official/systemc/archive/refs/tags/3.0.2.tar.gz
    URL_HASH SHA256=9b3693ed286aab958b9e5d79bb0ad3bc523bbc46931100553275352038f4a0c4
)

FetchContent_MakeAvailable(systemc)

set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    pysysc
    URL https://github.com/cphurley82/PySysC/archive/refs/heads/fix-pyeval-init.tar.gz
    URL_HASH SHA256=b5f377e22010ac39191ea80c463959c58549ca42df535d9368ab657a760258d8
)


FetchContent_GetProperties(pysysc)
if(NOT pysysc_POPULATED)
    FetchContent_Populate(pysysc)
endif()

if(NOT EXISTS "${pysysc_SOURCE_DIR}/PyScModule.h")
    message(FATAL_ERROR "PySysC header PyScModule.h not found. FetchContent download may have failed.")
endif()

add_library(pysysc_interface INTERFACE)
target_include_directories(
    pysysc_interface
    INTERFACE
        ${pysysc_SOURCE_DIR}
)

file(GLOB_RECURSE TOYSSD_HEADERS CONFIGURE_DEPENDS "include/toyssd/*.hpp")

set(TOYSSD_SOURCES
    src/host/host.cpp
    src/ssd_controller/controller.cpp
    src/nand/nand.cpp
    src/common/extensions.cpp
)

add_library(toyssd_core STATIC ${TOYSSD_HEADERS} ${TOYSSD_SOURCES})
target_include_directories(
    toyssd_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    toyssd_core
    PUBLIC
        SystemC::systemc
    PRIVATE
        pysysc_interface
)
target_compile_features(toyssd_core PUBLIC cxx_std_20)

if(TOYSSD_ENABLE_WARNINGS)
    include(EnableToyssdWarnings)
    toyssd_enable_warnings(toyssd_core)
endif()

add_subdirectory(examples)

if(TOYSSD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
