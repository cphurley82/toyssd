# simlib — core simulator library (SystemC components + firmware)
# This shared library contains most of the simulator implementation and is
# linked by the public-facing 'ssdsim' library and unit tests.
add_library(simlib SHARED
  host/HostInterface.cpp
  fw/Firmware.cpp fw/FTL.cpp
  nand/NandInterfaceImpl.cpp nand/NandModel.cpp
  Top.h
)
target_compile_features(simlib PUBLIC cxx_std_20)
add_dependencies(simlib format)
toyssd_apply_werror(simlib)
toyssd_apply_clang_tidy(simlib)

# Public include directories so downstream targets can include sim headers
# when they link to simlib. We also expose the C API headers from ../api for
# convenience while building from source.
target_include_directories(simlib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../api>
  $<INSTALL_INTERFACE:include/toyssd/sim>
  $<INSTALL_INTERFACE:include/toyssd>
)

target_link_libraries(simlib PUBLIC systemc)
set_target_properties(simlib PROPERTIES EXPORT_NAME simlib)
set_target_properties(simlib PROPERTIES
  # Tidy build tree layout: libraries -> lib/, executables -> bin/
  # Use $<CONFIG> so multi-config generators (Xcode/VS) keep configs separate.
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/lib"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/bin"
)

# sim_main — standalone SystemC entry-point
# We keep sc_main out of libraries to avoid accidental multiple-definition
# issues when tests or other binaries link against simlib. This small
# executable is handy for ad-hoc runs of the simulator.
add_executable(sim_main
  main.cpp
)
target_compile_features(sim_main PRIVATE cxx_std_20)
target_link_libraries(sim_main PRIVATE simlib systemc)
set_target_properties(sim_main PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/bin"
)
toyssd_apply_werror(sim_main)
toyssd_apply_clang_tidy(sim_main)

# Python adapter library (optional)
if(TOYSSD_BUILD_PYTHON)
  add_library(toyssd_pycore SHARED
    python/ToyssdPyAdapter.cpp
  )
  target_compile_features(toyssd_pycore PUBLIC cxx_std_20)
  target_include_directories(toyssd_pycore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/toyssd/sim>
  )
  target_link_libraries(toyssd_pycore PRIVATE simlib systemc scmain_stub)
  add_dependencies(toyssd_pycore systemc_ext)
  if(APPLE)
    set(_toyssd_py_rpath "@loader_path;@loader_path/../lib;@loader_path/../_vendor/systemc/lib")
  else()
    set(_toyssd_py_rpath "$ORIGIN;$ORIGIN/../lib;$ORIGIN/../_vendor/systemc/lib")
  endif()
  set_target_properties(toyssd_pycore PROPERTIES
    INSTALL_RPATH "${_toyssd_py_rpath}"
    BUILD_RPATH "${_toyssd_py_rpath}"
  )
  set_target_properties(toyssd_pycore PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/bin"
    EXPORT_NAME toyssd_pycore
  )
  toyssd_apply_werror(toyssd_pycore)
  toyssd_apply_clang_tidy(toyssd_pycore)
endif()

# Subdirectory for NAND-specific configuration and tests
add_subdirectory(nand)
