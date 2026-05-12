#include "config.h"
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>

namespace silent_storm {

namespace {
std::string strip(std::string s) {
    auto first = s.find_first_not_of(" \t\r\n");
    auto last = s.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, last - first + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

WindowMode parseMode(const std::string& v) {
    auto l = lower(v);
    if (l == "windowed") return WindowMode::Windowed;
    if (l == "fullscreen") return WindowMode::Fullscreen;
    return WindowMode::FullscreenDesktop;
}

Backend parseBackend(const std::string& v) {
    auto l = lower(v);
    if (l == "d3d11") return Backend::D3D11;
    if (l == "d3d12") return Backend::D3D12;
    if (l == "vulkan") return Backend::Vulkan;
    if (l == "metal") return Backend::Metal;
    if (l == "opengl") return Backend::OpenGL;
    return Backend::Auto;
}

bool parseBool(const std::string& v) {
    auto l = lower(v);
    return l == "on" || l == "true" || l == "yes" || l == "1";
}
} // namespace

Config LoadConfig(const std::string& path) {
    Config cfg;
    std::ifstream f(path);
    if (!f) return cfg;

    std::string section, line;
    while (std::getline(f, line)) {
        line = strip(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = lower(line.substr(1, line.size() - 2));
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = lower(strip(line.substr(0, eq)));
        auto val = strip(line.substr(eq + 1));

        if (section == "display") {
            if (key == "width") cfg.display.width = std::stoi(val);
            else if (key == "height") cfg.display.height = std::stoi(val);
            else if (key == "mode") cfg.display.mode = parseMode(val);
            else if (key == "vsync") cfg.display.vsync = parseBool(val);
            else if (key == "fov_horizontal") {
                if (lower(val) != "auto") cfg.display.fov_horizontal = std::stof(val);
            }
            else if (key == "hud_scale") {
                if (lower(val) != "auto") cfg.display.hud_scale = std::stoi(val);
            }
        } else if (section == "renderer") {
            if (key == "backend") cfg.renderer.backend = parseBackend(val);
        }
    }
    return cfg;
}

} // namespace silent_storm
