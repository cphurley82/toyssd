## Centralized warnings policy for toyssd
#
# Encapsulates our warning flags to keep the top-level concise
# and applies a consistent policy to all targets that opt in.
#
# Philosophy:
# - Be strict enough to catch real bugs early (conversion/sign issues).
# - Keep noise manageable so warnings stay actionable.
# - Avoid forcing consumers to inherit our policy unless they choose to.

function(toyssd_enable_warnings target)
    if(MSVC)
        # /permissive- enables standards-conforming behavior in MSVC.
        # /W4 selects a high warning level that's still practical for CI.
        target_compile_options(${target} PRIVATE /permissive- /W4)
    else()
        # GCC/Clang common warning set:
        # -Wall -Wextra: broad baseline for common mistakes.
        # -Wpedantic: enforce standard-conforming code (no compiler extensions).
        # -Wconversion -Wsign-conversion: highlight implicit/narrowing casts and
        #   signed/unsigned mismatches that often bite at boundaries.
        # -Wdeprecated: surface uses of deprecated APIs early so we can migrate.
        # TODO(cphurley): Increase warnings to maximum sensible set.
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Wdeprecated
        )
    endif()
endfunction()
