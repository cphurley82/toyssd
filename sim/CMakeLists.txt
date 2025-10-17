add_library(simlib
  host/HostInterface.cpp
  fw/Firmware.cpp fw/FTL.cpp
  nand/NandInterfaceImpl.cpp nand/NandModel.cpp
  Top.h
)

target_include_directories(simlib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../api>
  $<INSTALL_INTERFACE:include/toyssd/sim>
  $<INSTALL_INTERFACE:include/toyssd>
)

target_link_libraries(simlib PUBLIC systemc)
set_target_properties(simlib PROPERTIES EXPORT_NAME simlib)

# Provide a standalone SystemC entry-point as a separate executable to avoid
# pulling sc_main into libraries that are linked by tests.
add_executable(sim_main
  main.cpp
)
target_link_libraries(sim_main PRIVATE simlib systemc)
