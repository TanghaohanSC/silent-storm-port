// shader_registry.h
// Phase 1 Task 9 — load the 8 compiled shader archetypes (vs+fs .bin files
// produced by bgfx's shaderc) into bgfx programs, keyed by archetype name.
#pragma once
#include <bgfx/bgfx.h>

namespace silent_storm::renderer {

// Initialize the registry by reading vs_<name>.bin + fs_<name>.bin pairs from
// `shader_dir/<archetype>/`. Falls back to the dev build path
// `build/msvc-debug/shaders/` if the relative path is missing.
//
// MUST be called after bgfx::init succeeds and before any draw call.
// Returns true if at least one archetype loaded.
bool load_all_archetypes(const char* shader_dir);

// Returns BGFX_INVALID_HANDLE if `archetype` is unknown or the registry
// has not been initialized.
bgfx::ProgramHandle get_program(const char* archetype);

// Get the sampler uniform for texture stage `stage` (0..7).  Each stage
// has its own uniform created lazily on first call.
bgfx::UniformHandle get_sampler_uniform(unsigned stage);

// Releases all bgfx programs + shaders + uniforms. Call before bgfx::shutdown.
void shutdown_registry();

} // namespace silent_storm::renderer
