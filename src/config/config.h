#pragma once
#include <string>
#include <cstdint>

namespace silent_storm {

enum class WindowMode { Windowed, Fullscreen, FullscreenDesktop };
enum class Backend { Auto, D3D11, D3D12, Vulkan, Metal, OpenGL };

struct DisplayCfg {
    int width = 0;          // 0 = desktop
    int height = 0;
    WindowMode mode = WindowMode::FullscreenDesktop;
    bool vsync = true;
    float fov_horizontal = 0.0f;  // 0 = auto (90° wide-angle)
    int hud_scale = 0;            // 0 = auto
};

struct RendererCfg {
    Backend backend = Backend::Auto;
};

struct Config {
    DisplayCfg display;
    RendererCfg renderer;
};

// Load a cfg file. If the file doesn't exist, returns Config with defaults.
// Malformed lines are silently ignored — game must start even with a broken cfg.
Config LoadConfig(const std::string& path);

} // namespace silent_storm
