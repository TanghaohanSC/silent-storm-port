#include "../../src/config/config.h"
#include <cassert>
#include <fstream>
#include <cstdio>

int main() {
    // Write a test cfg
    {
        std::ofstream f("test.cfg");
        f << "[display]\n"
             "width = 1920\n"
             "height = 1080\n"
             "mode = fullscreen-desktop\n"
             "vsync = on\n"
             "fov_horizontal = 90\n"
             "hud_scale = 2\n"
             "[renderer]\n"
             "backend = vulkan\n";
    }

    silent_storm::Config c = silent_storm::LoadConfig("test.cfg");

    assert(c.display.width == 1920);
    assert(c.display.height == 1080);
    assert(c.display.mode == silent_storm::WindowMode::FullscreenDesktop);
    assert(c.display.vsync == true);
    assert(c.display.fov_horizontal == 90.0f);
    assert(c.display.hud_scale == 2);
    assert(c.renderer.backend == silent_storm::Backend::Vulkan);

    std::remove("test.cfg");
    return 0;
}
