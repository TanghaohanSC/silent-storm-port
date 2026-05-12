# Per Phase 1 plan Task 8 — bgfx shader pipeline.
# Each archetype dir provides varying.def.sc + vs_<name>.sc + fs_<name>.sc.
# bgfx's shaderc tool (built via BGFX_BUILD_TOOLS=ON) compiles to bin format.

function(compile_shader name)
    set(out_dir ${CMAKE_BINARY_DIR}/shaders/${name})
    file(MAKE_DIRECTORY ${out_dir})
    set(vs_src ${CMAKE_CURRENT_SOURCE_DIR}/${name}/vs_${name}.sc)
    set(fs_src ${CMAKE_CURRENT_SOURCE_DIR}/${name}/fs_${name}.sc)
    set(varying ${CMAKE_CURRENT_SOURCE_DIR}/${name}/varying.def.sc)
    set(vs_out ${out_dir}/vs_${name}.bin)
    set(fs_out ${out_dir}/fs_${name}.bin)
    set(bgfx_inc ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/bgfx/bgfx/src)
    add_custom_command(
        OUTPUT ${vs_out}
        COMMAND $<TARGET_FILE:shaderc>
            --type vertex --platform windows --profile s_5_0
            -i ${bgfx_inc}
            --varyingdef ${varying} -f ${vs_src} -o ${vs_out}
        DEPENDS ${vs_src} ${varying} shaderc
        COMMENT "shaderc vertex ${name}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${fs_out}
        COMMAND $<TARGET_FILE:shaderc>
            --type fragment --platform windows --profile s_5_0
            -i ${bgfx_inc}
            --varyingdef ${varying} -f ${fs_src} -o ${fs_out}
        DEPENDS ${fs_src} ${varying} shaderc
        COMMENT "shaderc fragment ${name}"
        VERBATIM
    )
    add_custom_target(shader_${name} DEPENDS ${vs_out} ${fs_out})
endfunction()

compile_shader(ss_diffuse_unlit)
compile_shader(ss_diffuse_lit)
compile_shader(ss_skinned)
compile_shader(ss_ui)
compile_shader(ss_particle)
compile_shader(ss_shadow)
compile_shader(ss_terrain)
compile_shader(ss_water)

add_custom_target(ss_shaders DEPENDS
    shader_ss_diffuse_unlit
    shader_ss_diffuse_lit
    shader_ss_skinned
    shader_ss_ui
    shader_ss_particle
    shader_ss_shadow
    shader_ss_terrain
    shader_ss_water
)