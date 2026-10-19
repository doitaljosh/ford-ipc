#include "ford_ipc/app_config.hpp"
#include "ford_ipc/layout.hpp"
#include "ford_ipc/time.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fordadas {

void printUsage(const char* argv0) {
    std::cout
        << "Ford IPC-style multi-page tester for GE GEA-SBC001 / Genio 350\n\n"
        << "Usage: " << argv0 << " [--demo | --can can0 | --replay candump.log]\n"
        << "  (no source)                    Standby: 0 speed/RPM, P selected, other fields dashed\n"
        << "  --can IFACE                    One vehicle CAN bus\n"
        << "  --rotate auto|0|90|180|270    Panel rotation\n"
        << "  --renderer NAME                SDL renderer; default opengles2\n"
        << "  --assets PATH                  Ford IPC asset directory\n"
        << "  --font-dir PATH                Ford Antenna fonts extracted from IPC firmware\n"
        << "  --render-scale auto|N          Internal supersampling scale\n"
        << "  --media-file PATH              key=value now-playing metadata file\n"
        << "  --chime-volume 0..100          Local Ford-style chime level; default 72\n"
        << "  --mute-chimes                  Disable local chime rendering\n"
        << "  --windowed                     1280x800 window instead of fullscreen desktop\n"
        << "  --fps N                        Render cap, default 60\n"
        << "  --distance-units auto|m|ft\n"
        << "  --replay-speed X\n"
        << "  --no-loop\n"
        << "  --debug\n";
}

Config parseArgs(int argc, char** argv) {
    Config c{};
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + a);
            return argv[++i];
        };
        if (a == "--demo") c.demo = true;
        else if (a == "--can") {
            if (!c.can_iface.empty()) throw std::runtime_error("only one --can interface is supported");
            c.can_iface = next();
        } else if (a == "--replay") c.replay = next();
        else if (a == "--replay-speed") c.replay_speed = std::stod(next());
        else if (a == "--no-loop") c.replay_loop = false;
        else if (a == "--windowed") c.windowed = true;
        else if (a == "--debug") c.debug = true;
        else if (a == "--fps") c.fps = std::max(10, std::min(60, std::stoi(next())));
        else if (a == "--distance-units") c.distance_units = next();
        else if (a == "--renderer") c.renderer = next();
        else if (a == "--assets") c.assets_dir = next();
        else if (a == "--font-dir") c.font_dir = next();
        else if (a == "--render-scale") { const auto v = next(); c.render_scale = v == "auto" ? 0.0 : std::stod(v); }
        else if (a == "--media-file") c.media_file = next();
        else if (a == "--chime-volume") c.chime_volume = std::max(0, std::min(100, std::stoi(next())));
        else if (a == "--mute-chimes") c.chimes_enabled = false;
        else if (a == "--rotate") { const auto v = next(); c.rotation = v == "auto" ? -1 : std::stoi(v); }
        else if (a == "-h" || a == "--help") { printUsage(argv[0]); std::exit(0); }
        else throw std::runtime_error("unknown option: " + a);
    }
    if (!c.can_iface.empty() || !c.replay.empty()) c.demo = false;
    return c;
}

std::filesystem::path resolveAssets(const Config& cfg, const char* argv0) {
    if (!cfg.assets_dir.empty()) return cfg.assets_dir;
    if (const char* env = std::getenv("FORD_ADAS_ASSETS")) return env;
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back("assets/ford_ipc");
    try {
        const auto exe = std::filesystem::canonical(argv0).parent_path();
        candidates.push_back(exe / "assets/ford_ipc");
        candidates.push_back(exe / "../assets/ford_ipc");
    } catch (...) {}
    char* base = SDL_GetBasePath();
    if (base != nullptr) {
        const std::filesystem::path p(base);
        candidates.push_back(p / "assets/ford_ipc");
        candidates.push_back(p / "../assets/ford_ipc");
        SDL_free(base);
    }
    for (const auto& p : candidates) if (std::filesystem::exists(p)) return p;
    return "assets/ford_ipc";
}

std::filesystem::path resolveFonts(const Config& cfg, const std::filesystem::path& assetPath) {
    if (!cfg.font_dir.empty()) return cfg.font_dir;
    if (const char* env = std::getenv("FORD_IPC_FONT_DIR")) return env;
    return assetPath / "private_fonts";
}

double chooseRasterScale(const Config& cfg, int outputW, int outputH, int rotation) noexcept {
    if (cfg.render_scale > 0.0) return clampd(cfg.render_scale, 1.0, 4.0);
    const bool quarter = rotation == 90 || rotation == 270;
    const double bw = quarter ? LogicalHeight : LogicalWidth;
    const double bh = quarter ? LogicalWidth : LogicalHeight;
    const double presentScale = std::min(outputW / bw, outputH / bh);
    return clampd(std::ceil(std::max(1.0, presentScale) * 2.0) / 2.0, 1.0, 4.0);
}

int normalizedRotation(int r) noexcept {
    r %= 360;
    if (r < 0) r += 360;
    return (r == 0 || r == 90 || r == 180 || r == 270) ? r : 0;
}

} // namespace fordadas
