# simlib — core simulator library (SystemC components + firmware)
# WHAT: Shared library with the bulk of the simulator implementation.
# WHY: Keep implementation details separate from the small C API surface so
#   consumers can link to only what they need, and tests can reuse internals.
add_library(simlib SHARED
  config.cc
  host/host_interface.cc
  fw/firmware.cc fw/ftl.cc
  nand/nand_interface_impl.cc nand/nand_model.cc
  top.h
)
target_compile_features(simlib PUBLIC cxx_std_20)
add_dependencies(simlib format)
# Ensure SystemC is built before compiling files that include <systemc> headers.
add_dependencies(simlib systemc_ext)
toyssd_apply_werror(simlib)
toyssd_apply_clang_tidy(simlib)

# Public include directories so downstream targets can include sim headers
# when they link to simlib.
# WHY: Expose headers in a build-tree-friendly way and install-tree-friendly
#   way without leaking absolute source paths to consumers.
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
# WHY: Keep sc_main out of libraries to avoid accidental multiple-definition
#   issues when tests or other binaries link against simlib. This small
#   executable is handy for ad-hoc runs of the simulator.
add_executable(sim_main main.cc)
target_compile_features(sim_main PRIVATE cxx_std_20)
target_link_libraries(sim_main PRIVATE simlib systemc)
add_dependencies(sim_main systemc_ext)
set_target_properties(sim_main PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/bin"
)
toyssd_apply_werror(sim_main)
toyssd_apply_clang_tidy(sim_main)

# Python adapter library (optional)
if(TOYSSD_BUILD_PYTHON)
  add_library(toyssd_pycore SHARED python/toyssd_py_adapter.cc)
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
