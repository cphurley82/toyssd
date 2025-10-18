# simlib — core simulator library (SystemC components + firmware)
# This shared library contains most of the simulator implementation and is
# linked by the public-facing 'ssdsim' library and unit tests.
add_library(simlib SHARED
  host/HostInterface.cpp
  fw/Firmware.cpp fw/FTL.cpp
  nand/NandInterfaceImpl.cpp nand/NandModel.cpp
  Top.h
)

# Public include directories so downstream targets can include sim headers
# when they link to simlib. We also expose the C API headers from ../api for
# convenience while building from source.
# TODO: Do we need the INSTALL_INTERFACE paths here if we are not installing?
target_include_directories(simlib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../api>
  $<INSTALL_INTERFACE:include/toyssd/sim>
  $<INSTALL_INTERFACE:include/toyssd>
)

target_link_libraries(simlib PUBLIC systemc)
set_target_properties(simlib PROPERTIES EXPORT_NAME simlib)
set_target_properties(simlib PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} # TODO: What are these?
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# sim_main — standalone SystemC entry-point
# We keep sc_main out of libraries to avoid accidental multiple-definition
# issues when tests or other binaries link against simlib. This small
# executable is handy for ad-hoc runs of the simulator.
add_executable(sim_main
  main.cpp
)
target_link_libraries(sim_main PRIVATE simlib systemc)
