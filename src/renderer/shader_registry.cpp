// shader_registry.cpp — Phase 1 Task 9
//
// Reads compiled shader .bin files (output of bgfx's shaderc) for each of the
// 8 archetypes, wraps them in bgfx::Memory and creates bgfx::Program handles.
//
// Layout expected:
//   <shader_dir>/<archetype>/vs_<archetype>.bin
//   <shader_dir>/<archetype>/fs_<archetype>.bin
//
// where <archetype> ∈ { ss_diffuse_unlit, ss_diffuse_lit, ss_skinned, ss_ui,
//                       ss_particle, ss_shadow, ss_terrain, ss_water }.
#include "shader_registry.h"
#include <bgfx/bgfx.h>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
FILE* open_log() {
    static FILE* f = nullptr;
    if (!f) fopen_s(&f, "silent_storm_shader.log", "w");
    return f;
}
void log_line(const char* fmt, ...) {
    FILE* f = open_log(); if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}
}

namespace silent_storm::renderer {

namespace {

constexpr const char* k_archetypes[] = {
    "ss_diffuse_unlit",
    "ss_diffuse_lit",
    "ss_skinned",
    "ss_ui",
    "ss_particle",
    "ss_shadow",
    "ss_terrain",
    "ss_water",
};

struct ArchetypeEntry {
    bgfx::ShaderHandle  vs{BGFX_INVALID_HANDLE};
    bgfx::ShaderHandle  fs{BGFX_INVALID_HANDLE};
    bgfx::ProgramHandle prog{BGFX_INVALID_HANDLE};
};

std::unordered_map<std::string, ArchetypeEntry> g_programs;
bgfx::UniformHandle g_samplers[8] = {
    BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
    BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
};

// Reads an entire file into a heap-allocated bgfx::Memory.
const bgfx::Memory* read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return nullptr;
    auto end = in.tellg();
    in.seekg(0);
    if (end <= 0) return nullptr;
    auto size = static_cast<uint32_t>(end);
    const bgfx::Memory* mem = bgfx::alloc(size + 1);
    if (!in.read(reinterpret_cast<char*>(mem->data), size)) return nullptr;
    mem->data[size] = '\0'; // bgfx convention: shader blobs are null-terminated
    return mem;
}

bool try_load_archetype(const std::filesystem::path& root, const char* name) {
    auto vs_path = root / name / (std::string("vs_") + name + ".bin");
    auto fs_path = root / name / (std::string("fs_") + name + ".bin");
    const bgfx::Memory* vs_mem = read_file(vs_path);
    const bgfx::Memory* fs_mem = read_file(fs_path);
    if (!vs_mem || !fs_mem) {
        log_line( "[shader_registry] %s: missing %s or %s\n",
                     name, vs_path.string().c_str(), fs_path.string().c_str());
        return false;
    }
    ArchetypeEntry e;
    e.vs   = bgfx::createShader(vs_mem);
    e.fs   = bgfx::createShader(fs_mem);
    if (!bgfx::isValid(e.vs) || !bgfx::isValid(e.fs)) {
        log_line( "[shader_registry] %s: createShader failed\n", name);
        if (bgfx::isValid(e.vs)) bgfx::destroy(e.vs);
        if (bgfx::isValid(e.fs)) bgfx::destroy(e.fs);
        return false;
    }
    e.prog = bgfx::createProgram(e.vs, e.fs, /*destroyShaders*/ false);
    if (!bgfx::isValid(e.prog)) {
        log_line( "[shader_registry] %s: createProgram failed\n", name);
        bgfx::destroy(e.vs);
        bgfx::destroy(e.fs);
        return false;
    }
    g_programs[name] = e;
    return true;
}

} // namespace

bool load_all_archetypes(const char* shader_dir) {
    namespace fs = std::filesystem;
    // Try caller path first, then fall back to absolute dev build path.
    std::vector<fs::path> candidates;
    if (shader_dir && *shader_dir) candidates.emplace_back(shader_dir);
    candidates.emplace_back("shaders");
    candidates.emplace_back(R"(C:\Users\Haohan\Documents\silent-storm\port\build\msvc-debug\shaders)");

    fs::path root;
    bool found_root = false;
    for (const auto& c : candidates) {
        if (fs::exists(c) && fs::is_directory(c)) {
            root = c;
            found_root = true;
            break;
        }
    }
    if (!found_root) {
        log_line( "[shader_registry] no shader dir found (tried %zu)\n", candidates.size());
        return false;
    }

    int loaded = 0;
    for (const char* name : k_archetypes) {
        if (try_load_archetype(root, name)) ++loaded;
    }
    log_line("[shader_registry] loaded %d/%d archetypes from %s",
             loaded, int(sizeof(k_archetypes)/sizeof(*k_archetypes)),
             root.string().c_str());

    // Phase 1.5 r2 iter 5: verify the registry round-trip works for every
    // archetype name string. (Catches typos / case-mismatches between the
    // load path and state_translator::select_shader_archetype.)
    for (const char* name : k_archetypes) {
        bgfx::ProgramHandle p = get_program(name);
        log_line("[shader_registry]   get_program(\"%s\") -> idx=%u valid=%d",
                 name, p.idx, (int)bgfx::isValid(p));
    }
    return loaded > 0;
}

bgfx::ProgramHandle get_program(const char* archetype) {
    if (!archetype) return BGFX_INVALID_HANDLE;
    auto it = g_programs.find(archetype);
    if (it == g_programs.end()) return BGFX_INVALID_HANDLE;
    return it->second.prog;
}

bgfx::UniformHandle get_sampler_uniform(unsigned stage) {
    if (stage >= 8) return BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(g_samplers[stage])) {
        char name[32];
        std::snprintf(name, sizeof(name), "s_diffuse%u", stage);
        // Stage 0 must match the shader uniform name `s_diffuse` (the fs shaders
        // use this name).  Other stages get a unique name so multiple uniforms
        // can coexist.
        const char* uname = (stage == 0) ? "s_diffuse" : name;
        g_samplers[stage] = bgfx::createUniform(uname, bgfx::UniformType::Sampler);
    }
    return g_samplers[stage];
}

void shutdown_registry() {
    for (auto& [name, e] : g_programs) {
        if (bgfx::isValid(e.prog)) bgfx::destroy(e.prog);
        if (bgfx::isValid(e.vs))   bgfx::destroy(e.vs);
        if (bgfx::isValid(e.fs))   bgfx::destroy(e.fs);
    }
    g_programs.clear();
    for (auto& u : g_samplers) {
        if (bgfx::isValid(u)) bgfx::destroy(u);
        u = BGFX_INVALID_HANDLE;
    }
}

} // namespace silent_storm::renderer
