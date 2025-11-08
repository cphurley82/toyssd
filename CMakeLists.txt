## Top-level CMake configuration for toyssd
#
# Orchestrates a mixed C++/SystemC toolchain with third-party sources fetched
# at configure time (SystemC, GoogleTest, PySysC).
# This file emphasizes reproducibility (pinned versions + hashes), portability,
# and good IDE integration (compile_commands).

cmake_minimum_required(VERSION 3.24)

project(
    toyssd
    VERSION 0.1.0
    LANGUAGES CXX
)

# Enforce a consistent C++ language mode across compilers and IDEs.
# - No compiler extensions for portability and cleaner diagnostics.
# - Export compile_commands.json to power IDE tooling (clangd, VS Code, etc.).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build toggles exposed to developers and CI. Warnings can be dialed up without
# forcing consumers of the library to adopt the same policy.
option(TOYSSD_BUILD_TESTS "Build toyssd unit tests" ON)
option(TOYSSD_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

# Add our custom CMake modules to the search path (e.g., warnings setup).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

# Keep older behavior for CMP0169 to retain compatibility with certain third-
# party projects or toolchain quirks (e.g., SystemC). Pinning policy state
# avoids surprising behavior changes when CMake is upgraded on developer machines.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# On macOS, prefer building for the single active host architecture instead of
# a universal binary. This shortens build times and prevents SDK/linker issues
# when dependencies (like SystemC) are not built as universal libraries.
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(_toyssd_host_arch "${CMAKE_SYSTEM_PROCESSOR}")
        if(_toyssd_host_arch MATCHES "^(arm64|aarch64)$")
            set(_toyssd_host_arch "arm64")
        elseif(_toyssd_host_arch MATCHES "^(x86_64|amd64)$")
            set(_toyssd_host_arch "x86_64")
        else()
            message(WARNING "Unrecognized macOS architecture '${_toyssd_host_arch}'. Falling back to arm64.")
            set(_toyssd_host_arch "arm64")
        endif()
        set(CMAKE_OSX_ARCHITECTURES "${_toyssd_host_arch}" CACHE STRING "Target macOS architectures" FORCE)
    endif()
endif()

# --- Third-party: SystemC ---
# We fetch and build a specific SystemC release for reproducibility.
FetchContent_Declare(
    systemc
    URL https://github.com/accellera-official/systemc/archive/refs/tags/3.0.2.tar.gz
    URL_HASH SHA256=9b3693ed286aab958b9e5d79bb0ad3bc523bbc46931100553275352038f4a0c4
)

FetchContent_MakeAvailable(systemc)

# --- Third-party: GoogleTest ---
# Use a pinned GTest for stable test behavior. The "shared CRT" toggle matters
# mainly on Windows; we set it for cross-platform consistency.
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
)
FetchContent_MakeAvailable(googletest)

# --- Third-party: PySysC headers ---
# PySysC is used to bridge Python and SystemC. We only need header access in
# this project; the code links against SystemC directly. We assert the header
# exists so users get a clear error if the download fails.
# Currently we are fetching from a fork with a specific fix for Python > 3.9.
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

# Header-only interface to expose PySysC includes to our targets without
# introducing link-time dependencies.
add_library(pysysc_interface INTERFACE)
target_include_directories(
    pysysc_interface
    INTERFACE
        ${pysysc_SOURCE_DIR}
)

# Include headers in the library target for IDEs and installation exports.
# GLOB_RECURSE is acceptable here because the project is small and we enable
# CONFIGURE_DEPENDS to refresh on new headers when re-configuring.
# TODO(cphurley): Switch to explicit listing.
file(GLOB_RECURSE TOYSSD_HEADERS CONFIGURE_DEPENDS "include/toyssd/*.hpp")

# Primary library sources grouped by subsystem.
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
# Publicly state our language feature requirements so dependents inherit C++20.
target_compile_features(toyssd_core PUBLIC cxx_std_20)

if(TOYSSD_ENABLE_WARNINGS)
    # Centralized, consistent warning policy across compilers to catch issues
    # early without drowning in noise. Can be disabled by packagers if needed.
    include(EnableToyssdWarnings)
    toyssd_enable_warnings(toyssd_core)
endif()

# Build a tiny SystemC example as a smoke test for the toolchain and the
# SystemC package brought in above.
add_subdirectory(examples)

if(TOYSSD_BUILD_TESTS)
    # Enable CTest integration and add unit tests for the core models.
    enable_testing()
    add_subdirectory(tests)
endif()
