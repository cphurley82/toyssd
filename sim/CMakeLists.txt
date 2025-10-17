add_library(simlib
  host/HostInterface.cpp
  fw/Firmware.cpp fw/FTL.cpp
  nand/NandInterfaceImpl.cpp nand/NandModel.cpp
  Top.h
)

target_include_directories(simlib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../api
)

target_link_libraries(simlib PUBLIC systemc)

# Provide a standalone SystemC entry-point as a separate executable to avoid
# pulling sc_main into libraries that are linked by tests.
add_executable(sim_main
  main.cpp
)
target_link_libraries(sim_main PRIVATE simlib systemc)
