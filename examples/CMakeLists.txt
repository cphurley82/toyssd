add_executable(toyssd_systemc_hello systemc_hello.cpp)
target_link_libraries(toyssd_systemc_hello PRIVATE SystemC::systemc)

add_custom_target(toyssd_smoke ALL
    COMMENT "Build SystemC hello world example"
    DEPENDS toyssd_systemc_hello
)
