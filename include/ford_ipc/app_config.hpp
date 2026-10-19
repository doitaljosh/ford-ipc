#pragma once

#include <filesystem>
#include <string>

namespace fordadas {

struct Config {
    bool demo = false;
    bool windowed = false;
    int window_w = 1280;
    int window_h = 800;
    bool debug = false;
    std::string replay;
    double replay_speed = 1.0;
    bool replay_loop = true;
    std::string can_iface;
    int rotation = -1;
    int fps = 60;
    std::string distance_units = "auto";
    std::string renderer = "opengles2";
    std::string assets_dir;
    std::string font_dir;
    double render_scale = 0.0;
    std::string media_file;
    bool chimes_enabled = true;
    int chime_volume = 72;
};

void printUsage(const char* argv0);
Config parseArgs(int argc, char** argv);
std::filesystem::path resolveAssets(const Config& cfg, const char* argv0);
std::filesystem::path resolveFonts(const Config& cfg, const std::filesystem::path& assetPath);
double chooseRasterScale(const Config& cfg, int outputW, int outputH, int rotation) noexcept;
int normalizedRotation(int rotation) noexcept;

} // namespace fordadas
