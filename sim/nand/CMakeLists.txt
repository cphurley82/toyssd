# NAND subsystem directory
# WHY: Keep NAND-specific tests and helpers scoped under this folder to keep
#   the top-level sim/ library lean and test discovery straightforward.

# Only add tests when BUILD_TESTING is enabled
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
