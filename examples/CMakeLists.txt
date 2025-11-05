## Examples build for toyssd
#
# We compile a minimal SystemC program as a fast smoke test of the toolchain
# and the fetched SystemC package. Keeping this tiny and isolated gives a
# quick signal during CI without pulling the full project into the binary.

add_executable(toyssd_systemc_hello systemc_hello.cpp)
# Link explicitly against the imported SystemC target provided by FetchContent
# at the top-level. Using the target (instead of raw library names) keeps
# include paths and definitions correct across platforms.
target_link_libraries(toyssd_systemc_hello PRIVATE SystemC::systemc)

# Aggregate target to make this example part of the default build. Marked as a
# custom target so we don't add an extra binary beyond the example itself.
add_custom_target(toyssd_smoke ALL
    COMMENT "Build SystemC hello world example"
    DEPENDS toyssd_systemc_hello
)
