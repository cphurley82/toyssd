function(toyssd_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /permissive- /W4)
    else()
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
