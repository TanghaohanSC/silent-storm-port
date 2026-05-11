# Warning policy for code we author. Legacy Jan03 code is silenced
# separately (see VendoredJan03.cmake when added).

function(target_apply_compiler_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
            /utf-8
            /wd4100  # unreferenced formal parameter
            /wd4189  # local var initialized but unreferenced
            /wd4244  # conversion possible loss of data
            /wd4267  # size_t -> int conversion
            /wd4996  # deprecated CRT (sprintf etc.) — handle later
        )
        target_compile_definitions(${target} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
    endif()
endfunction()
