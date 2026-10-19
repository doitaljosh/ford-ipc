#include "ford_ipc/ui.hpp"
#include "ford_ipc/ford_decoder.hpp"
#include "ford_ipc/layout.hpp"
#include "ford_ipc/time.hpp"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fordadas {
namespace {

struct Color { Uint8 r, g, b, a = 255; };
constexpr Color BLACK{0, 0, 0, 255};
constexpr Color DEEP_BLUE{3, 12, 19, 255};
constexpr Color FORD_BLUE{45, 155, 205, 255};
constexpr Color ICE_BLUE{135, 208, 233, 255};
constexpr Color WHITE{231, 236, 239, 255};
constexpr Color MUTED{117, 128, 137, 255};
constexpr Color DARK_MUTED{42, 49, 55, 255};
constexpr Color AMBER{244, 151, 20, 255};
constexpr Color MIL_ORANGE{255, 150, 0, 255};
constexpr Color RED{238, 45, 52, 255};
constexpr Color GREEN{68, 197, 115, 255};
constexpr Color ROAD{8, 13, 17, 255};

void setColor(SDL_Renderer* r, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void fillRect(SDL_Renderer* r, int x, int y, int w, int h, Color c) {
    setColor(r, c);
    SDL_Rect rc{x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}

void drawRect(SDL_Renderer* r, int x, int y, int w, int h, Color c) {
    setColor(r, c);
    SDL_Rect rc{x, y, w, h};
    SDL_RenderDrawRect(r, &rc);
}

void thickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, int width, Color c) {
    setColor(r, c);
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double len = std::max(1.0, std::sqrt(dx * dx + dy * dy));
    const double nx = -dy / len;
    const double ny = dx / len;
    for (int i = -width / 2; i <= width / 2; ++i) {
        SDL_RenderDrawLine(r,
            static_cast<int>(std::lround(x1 + nx * i)),
            static_cast<int>(std::lround(y1 + ny * i)),
            static_cast<int>(std::lround(x2 + nx * i)),
            static_cast<int>(std::lround(y2 + ny * i)));
    }
}

void fillCircle(SDL_Renderer* r, int cx, int cy, int radius, Color c) {
    setColor(r, c);
    for (int y = -radius; y <= radius; ++y) {
        const int x = static_cast<int>(std::floor(std::sqrt(std::max(0, radius * radius - y * y))));
        SDL_RenderDrawLine(r, cx - x, cy + y, cx + x, cy + y);
    }
}

void drawCircle(SDL_Renderer* r, int cx, int cy, int radius, int width, Color c) {
    constexpr double PI = 3.14159265358979323846;
    int px = cx + radius, py = cy;
    for (int i = 1; i <= 96; ++i) {
        const double a = 2.0 * PI * i / 96.0;
        const int x = static_cast<int>(std::lround(cx + std::cos(a) * radius));
        const int y = static_cast<int>(std::lround(cy + std::sin(a) * radius));
        thickLine(r, px, py, x, y, width, c);
        px = x; py = y;
    }
}

void triangle(SDL_Renderer* r, SDL_Point a, SDL_Point b, SDL_Point c, Color col) {
    int miny = std::min({a.y, b.y, c.y});
    int maxy = std::max({a.y, b.y, c.y});
    setColor(r, col);
    auto edgeX = [](SDL_Point p1, SDL_Point p2, int y) -> std::optional<double> {
        if (p1.y == p2.y) return std::nullopt;
        if (y < std::min(p1.y, p2.y) || y > std::max(p1.y, p2.y)) return std::nullopt;
        return p1.x + (double)(y - p1.y) * (p2.x - p1.x) / (double)(p2.y - p1.y);
    };
    for (int y = miny; y <= maxy; ++y) {
        std::vector<double> xs;
        for (auto e : {std::pair{a, b}, std::pair{b, c}, std::pair{c, a}}) {
            if (auto x = edgeX(e.first, e.second, y)) xs.push_back(*x);
        }
        if (xs.size() >= 2) {
            std::sort(xs.begin(), xs.end());
            SDL_RenderDrawLine(r, (int)std::lround(xs.front()), y, (int)std::lround(xs.back()), y);
        }
    }
}

class TextCache {
public:
    enum class Face { Light = 0, Regular = 1 };

    TextCache(SDL_Renderer* r, std::filesystem::path fontRoot, double rasterScale)
        : renderer_(r), root_(std::move(fontRoot)), raster_scale_(std::max(1.0, rasterScale)) {
        light_path_ = root_ / "FordAntennaCondWGL-Light.otf";
        regular_path_ = root_ / "FordAntennaCondWGL-Reg.otf";
        if (!std::filesystem::exists(light_path_) || !std::filesystem::exists(regular_path_)) {
            std::ostringstream ss;
            ss << "Ford Antenna cluster fonts are missing from " << root_ << "\\n"
               << "Run extract_cluster_fonts.py against your LV4T-14C088-AJ IPC firmware.";
            throw std::runtime_error(ss.str());
        }
    }

    ~TextCache() {
        clearTextCache();
        for (auto& [_, f] : fonts_) if (f) TTF_CloseFont(f);
    }

    void draw(const std::string& text, int x, int y, int size, Color color,
              bool bold = false, bool center = false, bool right = false) {
        drawWith(text, x, y, size, color, bold ? Face::Regular : Face::Light, center, right);
    }

    void drawSign(const std::string& text, int x, int y, int size, Color color,
                  bool center = false, bool right = false) {
        // Keep the same Ford Antenna family globally, including TSR/sign text.
        drawWith(text, x, y, size, color, Face::Regular, center, right);
    }

private:
    struct CachedText {
        SDL_Texture* texture = nullptr;
        int pixel_w = 0;
        int pixel_h = 0;
        int logical_w = 0;
        int logical_h = 0;
    };

    SDL_Renderer* renderer_ = nullptr;
    std::filesystem::path root_;
    std::filesystem::path light_path_;
    std::filesystem::path regular_path_;
    double raster_scale_ = 1.0;
    std::map<std::pair<int,int>, TTF_Font*> fonts_;
    std::unordered_map<std::string, CachedText> text_cache_;

    const std::filesystem::path& pathFor(Face face) const {
        if (face == Face::Light) return light_path_;
        return regular_path_;
    }

    TTF_Font* fontFor(Face face, int logicalSize) {
        const int px = std::max(6, static_cast<int>(std::lround(logicalSize * raster_scale_)));
        const auto key = std::make_pair(static_cast<int>(face), px);
        auto it = fonts_.find(key);
        if (it != fonts_.end()) return it->second;

        TTF_Font* font = TTF_OpenFont(pathFor(face).string().c_str(), px);
        if (!font) {
            throw std::runtime_error("cannot open Ford Antenna font " + pathFor(face).string() + ": " + TTF_GetError());
        }
        TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
        TTF_SetFontKerning(font, 1);
        fonts_[key] = font;
        return font;
    }

    static std::string cacheKey(Face face, int px, const std::string& text) {
        return std::to_string(static_cast<int>(face)) + ":" + std::to_string(px) + ":" + text;
    }

    void clearTextCache() {
        for (auto& [_, t] : text_cache_) if (t.texture) SDL_DestroyTexture(t.texture);
        text_cache_.clear();
    }

    CachedText& rasterize(Face face, int logicalSize, const std::string& text) {
        const int px = std::max(6, static_cast<int>(std::lround(logicalSize * raster_scale_)));
        const std::string key = cacheKey(face, px, text);
        auto it = text_cache_.find(key);
        if (it != text_cache_.end()) return it->second;

        // Dynamic values such as speed/RPM can create many unique strings. Keep
        // the cache bounded instead of growing for the lifetime of the process.
        if (text_cache_.size() >= 1024) clearTextCache();

        TTF_Font* font = fontFor(face, logicalSize);
        SDL_Color white{255,255,255,255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), white);
        if (!surf) throw std::runtime_error("Ford Antenna text render failed: " + std::string(TTF_GetError()));

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surf);
        if (!texture) {
            SDL_FreeSurface(surf);
            throw std::runtime_error("text texture creation failed: " + std::string(SDL_GetError()));
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        CachedText item;
        item.texture = texture;
        item.pixel_w = surf->w;
        item.pixel_h = surf->h;
        item.logical_w = std::max(1, static_cast<int>(std::lround(surf->w / raster_scale_)));
        item.logical_h = std::max(1, static_cast<int>(std::lround(surf->h / raster_scale_)));
        SDL_FreeSurface(surf);
        auto [inserted, _] = text_cache_.emplace(key, item);
        return inserted->second;
    }

    void drawWith(const std::string& text, int x, int y, int size, Color color,
                  Face face, bool center, bool right) {
        if (text.empty() || size <= 0) return;
        auto& t = rasterize(face, size, text);
        int dx = x;
        if (center) dx -= t.logical_w / 2;
        if (right) dx -= t.logical_w;
        SDL_Rect dst{dx, y, t.logical_w, t.logical_h};
        SDL_SetTextureColorMod(t.texture, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(t.texture, color.a);
        SDL_RenderCopy(renderer_, t.texture, nullptr, &dst);
        SDL_SetTextureColorMod(t.texture, 255,255,255);
        SDL_SetTextureAlphaMod(t.texture, 255);
    }
};

class AssetStore {
public:
    AssetStore(SDL_Renderer* renderer, std::filesystem::path root)
        : renderer_(renderer), root_(std::move(root)) {
        load("lane_left_white", "lane_left_white.png");
        load("lane_right_white", "lane_right_white.png");
        load("lane_left_green", "lane_left_green.png");
        load("lane_right_green", "lane_right_green.png");
        load("lane_left_red", "lane_left_red.png");
        load("lane_right_red", "lane_right_red.png");
        load("lane_left_amber", "lane_left_amber.png");
        load("lane_right_amber", "lane_right_amber.png");
        load("host_vehicle", "host_vehicle.png");
        load("lead_vehicle_near", "lead_vehicle_near.png");
        load("lead_vehicle_mid", "lead_vehicle_mid.png");
        load("lead_vehicle_far", "lead_vehicle_far.png");
        load("lead_vehicle_very_far", "lead_vehicle_very_far.png");
        load("follow_gap_1", "follow_gap_1.png");
        load("follow_gap_2", "follow_gap_2.png");
        load("follow_gap_3", "follow_gap_3.png");
        load("follow_gap_4", "follow_gap_4.png");
        load("follow_gap_green_1", "follow_gap_green_1.png");
        load("follow_gap_green_2", "follow_gap_green_2.png");
        load("follow_gap_green_3", "follow_gap_green_3.png");
        load("follow_gap_green_4", "follow_gap_green_4.png");
        load("speed_limit_blank", "speed_limit_blank.png");
        load("no_passing", "no_passing.png");
        load("ipc_background", "ipc_background.png");
        load("telltale_mil", "telltale_mil.png");
        load("telltale_abs", "telltale_abs.png");
        load("telltale_traction", "telltale_traction.png");
        load("telltale_oil", "telltale_oil.png");
        load("telltale_brake", "telltale_brake.png");
        load("telltale_battery", "telltale_battery.png");
        load("telltale_seatbelt", "telltale_seatbelt.png");
        load("telltale_tpms", "telltale_tpms.png");
        load("telltale_airbag", "telltale_airbag.png");
        load("gauge_speed_face", "gauge_speed_face.png");
        load("gauge_tach_face", "gauge_tach_face.png");
        load("gauge_needle_shadow", "gauge_needle_shadow.png");
        load("gauge_needle", "gauge_needle.png");
        load("gauge_hub", "gauge_hub.png");
        load("vehicle_base", "vehicle_base.png");
        load("vehicle_door_df", "vehicle_door_df.png");
        load("vehicle_door_pf", "vehicle_door_pf.png");
        load("vehicle_door_rl", "vehicle_door_rl.png");
        load("vehicle_door_rr", "vehicle_door_rr.png");
        load("vehicle_hood", "vehicle_hood.png");
        load("vehicle_liftgate", "vehicle_liftgate.png");
        load("vehicle_door_df_shadow", "vehicle_door_df_shadow.png");
        load("vehicle_door_pf_shadow", "vehicle_door_pf_shadow.png");
        load("vehicle_door_rl_shadow", "vehicle_door_rl_shadow.png");
        load("vehicle_door_rr_shadow", "vehicle_door_rr_shadow.png");
        load("vehicle_hood_shadow", "vehicle_hood_shadow.png");
        load("vehicle_liftgate_shadow", "vehicle_liftgate_shadow.png");
    }

    ~AssetStore() {
        for (auto& [_, t] : textures_) if (t) SDL_DestroyTexture(t);
    }

    bool has(const std::string& name) const {
        return textures_.find(name) != textures_.end();
    }

    void draw(const std::string& name, const SDL_Rect& dst, double angle = 0.0,
              SDL_RendererFlip flip = SDL_FLIP_NONE, Uint8 alpha = 255) {
        auto it = textures_.find(name);
        if (it == textures_.end()) return;
        SDL_SetTextureAlphaMod(it->second, alpha);
        SDL_RenderCopyEx(renderer_, it->second, nullptr, &dst, angle, nullptr, flip);
        if (alpha != 255) SDL_SetTextureAlphaMod(it->second, 255);
    }

    void drawPivot(const std::string& name, const SDL_Rect& dst, double angle,
                   double pivotSrcX, double pivotSrcY, Uint8 alpha = 255) {
        auto it = textures_.find(name);
        if (it == textures_.end()) return;
        int tw = 1, th = 1;
        SDL_QueryTexture(it->second, nullptr, nullptr, &tw, &th);
        SDL_Point pivot{
            static_cast<int>(std::lround((pivotSrcX / std::max(1, tw)) * dst.w)),
            static_cast<int>(std::lround((pivotSrcY / std::max(1, th)) * dst.h))
        };
        SDL_SetTextureAlphaMod(it->second, alpha);
        SDL_RenderCopyEx(renderer_, it->second, nullptr, &dst, angle, &pivot, SDL_FLIP_NONE);
        if (alpha != 255) SDL_SetTextureAlphaMod(it->second, 255);
    }

    const std::filesystem::path& root() const { return root_; }

private:
    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    std::unordered_map<std::string, SDL_Texture*> textures_;

    void load(const std::string& name, const std::string& file) {
        const auto path = root_ / file;
        SDL_Texture* t = IMG_LoadTexture(renderer_, path.string().c_str());
        if (!t) {
            throw std::runtime_error("required cluster asset missing/unreadable: " + path.string() + ": " + IMG_GetError());
        }
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        textures_[name] = t;
    }
};



Color alphaColor(Color c, Uint8 a) {
    c.a = static_cast<Uint8>((static_cast<unsigned>(c.a) * a) / 255U);
    return c;
}

void drawIntegratedVehicle(SDL_Renderer* r, AssetStore& assets,
                           int cx, int top, int w, int h,
                           const VehiclePanelVisual& v, Uint8 alpha = 255) {
    (void)r;
    SDL_Rect car{cx - w / 2, top, w, h};
    if (!assets.has("vehicle_base")) {
        assets.draw("host_vehicle", car, 0.0, SDL_FLIP_NONE, alpha);
        return;
    }

    assets.draw("vehicle_base", car, 0.0, SDL_FLIP_NONE, alpha);

    auto shadowAlpha = [alpha](double p) -> Uint8 {
        return static_cast<Uint8>(std::lround((alpha / 255.0) * clampd(p, 0.0, 1.0) * 105.0));
    };
    auto drawDoor = [&](const char* panel, const char* shadow, double p,
                        double pivotX, double pivotY, double maxAngle, int shadowDx) {
        if (p <= 0.001) {
            assets.draw(panel, car, 0.0, SDL_FLIP_NONE, alpha);
            return;
        }
        const double angle = maxAngle * smooth01(p);
        SDL_Rect sh = car;
        sh.x += static_cast<int>(std::lround(shadowDx * p));
        sh.y += 2;
        assets.drawPivot(shadow, sh, angle, pivotX, pivotY, shadowAlpha(p));
        assets.drawPivot(panel, car, angle, pivotX, pivotY, alpha);
    };

    // The panel textures are full-car transparent layers cut from the genuine
    // Ford top-down vehicle asset. Rotating them about real hinge locations
    // makes the open panel remain visually connected to the body.
    drawDoor("vehicle_door_df", "vehicle_door_df_shadow", v.driver_front, 27, 69, 55.0, -4);
    drawDoor("vehicle_door_pf", "vehicle_door_pf_shadow", v.passenger_front, 63, 69, -55.0, 4);
    drawDoor("vehicle_door_rl", "vehicle_door_rl_shadow", v.rear_left, 30, 116, 58.0, -4);
    drawDoor("vehicle_door_rr", "vehicle_door_rr_shadow", v.rear_right, 60, 116, -58.0, 4);

    // Hood and liftgate open mostly out of the screen plane in reality. A
    // short fore/aft translation plus a soft shadow reads much more naturally
    // in a top-down cluster graphic than an arbitrary 2-D hinge rotation.
    auto drawSlidingPanel = [&](const char* panel, const char* shadow, double p, int dy) {
        SDL_Rect d = car;
        d.y += static_cast<int>(std::lround(dy * smooth01(p)));
        if (p > 0.001) {
            SDL_Rect sh = d;
            sh.y += (dy < 0 ? 3 : -3);
            assets.draw(shadow, sh, 0.0, SDL_FLIP_NONE, shadowAlpha(p));
        }
        assets.draw(panel, d, 0.0, SDL_FLIP_NONE, alpha);
    };
    drawSlidingPanel("vehicle_hood", "vehicle_hood_shadow", v.hood, -24);
    drawSlidingPanel("vehicle_liftgate", "vehicle_liftgate_shadow", v.liftgate, 23);
}


const char* speedUnitText(SpeedUnit unit) noexcept {
    switch (unit) {
        case SpeedUnit::Unknown: return "";
        case SpeedUnit::Kph: return "km/h";
        case SpeedUnit::Mph: return "mph";
    }
    return "";
}

const char* restrictionText(SignRestriction restriction) noexcept {
    switch (restriction) {
        case SignRestriction::None: return "";
        case SignRestriction::Wet: return "WET";
        case SignRestriction::Snow: return "SNOW";
        case SignRestriction::Trailer: return "TRAILER";
        case SignRestriction::Time: return "TIME";
    }
    return "";
}

std::string distanceText(double m, SpeedUnit signUnit, const std::string& pref) {
    std::string units = pref;
    if (units == "auto") units = (signUnit == SpeedUnit::Mph) ? "ft" : "m";
    std::ostringstream ss;
    if (units == "ft") ss << std::fixed << std::setprecision(0) << m * 3.28084 << " ft";
    else ss << std::fixed << std::setprecision(0) << m << " m";
    return ss.str();
}

void drawFordHostVehicle(SDL_Renderer* r, AssetStore& assets, int cx, int cy, int w, int h) {
    if (assets.has("host_vehicle")) {
        SDL_Rect dst{cx - w / 2, cy - h / 2, w, h};
        assets.draw("host_vehicle", dst);
        return;
    }
    fillRect(r, cx - w / 2, cy - h / 2, w, h, WHITE);
}

std::string laneAssetName(bool left, const LaneState& lane, double now) {
    const bool fresh = lane.updated > 0 && now - lane.updated < 1.2;
    std::string side = left ? "left" : "right";
    if (!fresh) return "lane_" + side + "_white";
    const bool departing = (left && lane.departure_state == 2) || (!left && lane.departure_state == 4);
    if (departing) return "lane_" + side + (lane.departure_intensity >= 2 ? "_red" : "_amber");
    // The IPC uses color sparingly. Keep ordinary detected lanes white; green
    // is reserved here for a strongly active lane-keeping state if observed.
    if (lane.lka_state == 3) return "lane_" + side + "_green";
    return "lane_" + side + "_white";
}

std::string leadAssetForDistance(double m) {
    if (m < 18) return "lead_vehicle_near";
    if (m < 35) return "lead_vehicle_mid";
    if (m < 60) return "lead_vehicle_far";
    return "lead_vehicle_very_far";
}

std::string gapAssetForValue(int gap, bool green = false) {
    const std::string prefix = green ? "follow_gap_green_" : "follow_gap_";
    if (gap <= 1) return prefix + "1";
    if (gap == 2) return prefix + "2";
    if (gap == 3) return prefix + "3";
    return prefix + "4";
}

bool frameFresh(const ADASState& s, uint32_t id, double now, double timeout = 2.0) {
    if (s.mode_text == "DEMO" || s.mode_text == "STANDBY") return true;
    return s.freshness.isFresh(id, now, timeout);
}

bool useImperial(const ADASState& s) {
    // US Escape default when TSR has not yet supplied a unit.
    return s.signs.unit != SpeedUnit::Kph;
}

std::string gearLetter(const std::optional<int>& gear) {
    if (!gear) return "-";
    switch (*gear) {
        case 0: return "P";
        case 1: return "R";
        case 2: return "N";
        case 3: return "D";
        case 4: return "S";
        case 5: return "L";
        default:
            if (*gear >= 6 && *gear <= 11) return std::to_string(*gear - 5);
            return "-";
    }
}

std::string compassText(const ADASState& s) {
    static const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    if (s.vehicle.heading_deg) {
        double d = std::fmod(*s.vehicle.heading_deg + 360.0, 360.0);
        int idx = static_cast<int>(std::floor((d + 22.5) / 45.0)) & 7;
        return dirs[idx];
    }
    if (s.vehicle.compass_dir && *s.vehicle.compass_dir >= 0 && *s.vehicle.compass_dir <= 7)
        return dirs[*s.vehicle.compass_dir];
    return "--";
}

bool lampVisible(int state, double now) {
    if (state == 1) return true;
    if (state == 2) return std::fmod(now, 1.0) < 0.50;
    if (state == 3) return std::fmod(now, 0.40) < 0.20;
    return false;
}


void drawGauge(SDL_Renderer* r, TextCache& tx, AssetStore& assets,
               int cx, int cy, int radius, double needleValue, double displayValue,
               bool valid, double maxValue, const std::string& label,
               const std::string& unit, int majorStep, bool tach = false) {
    (void)r;
    constexpr double PI = 3.14159265358979323846;
    constexpr double START = 140.0;
    constexpr double SWEEP = 260.0;
    constexpr int FACE_SIZE = 360;

    // Preserve the original smooth GE/Asahi gauge treatment at the asset's
    // native 360x360 size.  The supersampled face contains the radial bowl
    // gradient, glass highlight, multi-ring metallic bezel and outer shadow.
    // Keeping it 1:1 avoids the 344x344 resample that flattened those details.
    SDL_Rect face{cx - FACE_SIZE / 2, cy - FACE_SIZE / 2, FACE_SIZE, FACE_SIZE};
    assets.draw(tach ? "gauge_tach_face" : "gauge_speed_face", face,
                0.0, SDL_FLIP_NONE, valid ? 255 : 150);

    auto pt = [&](double degrees, double rr) {
        const double a = degrees * PI / 180.0;
        return SDL_Point{static_cast<int>(std::lround(cx + std::cos(a) * rr)),
                         static_cast<int>(std::lround(cy + std::sin(a) * rr))};
    };

    // Numerals stay live so the extracted Ford Antenna face matches the rest
    // of the cluster while all anti-aliased ticks remain in the gauge texture.
    const int majors = std::max(1, static_cast<int>(std::lround(maxValue / majorStep)));
    const int tickFont = std::max(12, radius / 11);
    for (int i = 0; i <= majors; ++i) {
        const double v = std::min(maxValue, i * static_cast<double>(majorStep));
        const double deg = START + SWEEP * (v / maxValue);
        const bool red = tach && v >= 6000.0;
        std::ostringstream ls;
        ls << (tach ? static_cast<int>(v / 1000.0) : static_cast<int>(std::lround(v)));
        const SDL_Point lp = pt(deg, radius - 49);
        tx.draw(ls.str(), lp.x, lp.y - tickFont / 2, tickFont,
                red ? Color{245, 96, 99, 255} : MUTED, false, true);
    }

    if (valid) {
        const double clamped = clampd(needleValue, 0.0, maxValue);
        const double deg = START + SWEEP * (clamped / maxValue);

        // Separate blurred shadow sprite + specular needle layer restores the
        // depth of the pre-backport gauges without runtime blur work.
        SDL_Rect shadow{cx - FACE_SIZE / 2 + 3, cy - FACE_SIZE / 2 + 5,
                        FACE_SIZE, FACE_SIZE};
        assets.draw("gauge_needle_shadow", shadow, deg);
        assets.draw("gauge_needle", face, deg);
    }
    assets.draw("gauge_hub", face, 0.0, SDL_FLIP_NONE, valid ? 255 : 175);

    std::string number = "--";
    if (valid) {
        std::ostringstream ss;
        if (tach) ss << std::fixed << std::setprecision(1) << (displayValue / 1000.0);
        else ss << std::fixed << std::setprecision(0) << displayValue;
        number = ss.str();
    }

    tx.draw(label, cx, cy + 70, 17, ICE_BLUE, true, true);
    tx.draw(number, cx, cy + 101, tach ? 46 : 52, WHITE, true, true);
    tx.draw(unit, cx, cy + 153, 15, MUTED, false, true);
}

void drawCompactSideStatus(SDL_Renderer* r, TextCache& tx, AssetStore& assets,
                           const SideStatus& side, bool left, double now) {
    const int cx = left ? 371 : 909;
    const int cy = 548;
    const bool stale = side.updated <= 0 || now - side.updated > 1.4;
    const bool detected = !stale && side.blind_detect > 0;
    const bool alert = !stale && side.blind_alert > 0;
    const bool cta = !stale && side.cta_alert;
    const Color c = alert || cta ? RED : detected ? AMBER : DARK_MUTED;

    tx.draw(left ? "L" : "R", cx, cy - 91, 16, MUTED, true, true);
    drawFordHostVehicle(r, assets, cx, cy, 31, 67);
    const int adjx = left ? cx - 49 : cx + 49;
    if (detected || alert) {
        drawFordHostVehicle(r, assets, adjx, cy - 6, 27, 58);
        drawRect(r, adjx - 18, cy - 42, 36, 72, c);
    }
    for (int i = 0; i < 3; ++i) {
        const int dy = 10 + i * 10;
        if (left) thickLine(r, cx - 20, cy - 5, cx - 43 - i * 5, cy - dy, 3, c);
        else thickLine(r, cx + 20, cy - 5, cx + 43 + i * 5, cy - dy, 3, c);
    }
    if (stale) tx.draw("--", cx, cy + 47, 14, DARK_MUTED, false, true);
    else if (alert || detected) tx.draw("BLIS", cx, cy + 47, 16, c, true, true);
    if (cta) {
        if (left) triangle(r, {cx - 76, cy + 10}, {cx - 45, cy - 6}, {cx - 45, cy + 26}, RED);
        else triangle(r, {cx + 76, cy + 10}, {cx + 45, cy - 6}, {cx + 45, cy + 26}, RED);
        tx.draw("CTA", cx, cy + 70, 15, RED, true, true);
    }
}


void drawTelltales(SDL_Renderer* r, AssetStore& assets, const ADASState& s, double now) {
    (void)r;
    struct Lamp { const char* asset; bool on; int w; int h; };
    const Lamp lamps[] = {
        {"telltale_mil",      lampVisible(s.telltales.mil, now), 43, 29},
        {"telltale_abs",      lampVisible(s.telltales.abs_lamp, now) || s.telltales.abs_fault, 43, 34},
        {"telltale_traction", lampVisible(s.telltales.traction_lamp, now), 37, 37},
        {"telltale_oil",      s.telltales.oil_pressure, 48, 21},
        {"telltale_brake",    s.telltales.brake_fluid == 1, 58, 19},
    };
    constexpr int GAP = 20;
    int total = 0;
    int count = 0;
    for (const auto& l : lamps) if (l.on) { total += l.w; ++count; }
    if (count > 1) total += (count - 1) * GAP;
    int x = LogicalWidth / 2 - total / 2;
    for (const auto& l : lamps) {
        if (!l.on) continue;
        SDL_Rect dst{x, 20 + (42 - l.h) / 2, l.w, l.h};
        assets.draw(l.asset, dst);
        x += l.w + GAP;
    }
}



void drawPostTelltales(AssetStore& assets, Uint8 alpha) {
    if (alpha == 0) return;
    struct Lamp { const char* asset; int w; int h; };
    const Lamp lamps[] = {
        {"telltale_mil", 43, 29},
        {"telltale_abs", 43, 34},
        {"telltale_traction", 37, 37},
        {"telltale_oil", 48, 21},
        {"telltale_brake", 58, 19},
        {"telltale_battery", 45, 30},
        {"telltale_seatbelt", 31, 37},
        {"telltale_tpms", 38, 37},
        {"telltale_airbag", 40, 37},
    };
    constexpr int GAP = 17;
    int total = -GAP;
    for (const auto& l : lamps) total += l.w + GAP;
    int x = LogicalWidth / 2 - total / 2;
    for (const auto& l : lamps) {
        SDL_Rect dst{x, 18 + (42 - l.h) / 2, l.w, l.h};
        assets.draw(l.asset, dst, 0.0, SDL_FLIP_NONE, alpha);
        x += l.w + GAP;
    }
}


void drawStatusBand(SDL_Renderer* r, TextCache& tx, const ADASState& s, double now) {
    const bool imperial = useImperial(s);
    fillRect(r, 56, 686, 1168, 101, {0, 5, 9, 210});
    thickLine(r, 56, 685, 1224, 685, 2, {31, 83, 106, 230});

    auto field = [&](int x, const std::string& top, const std::string& bottom, bool emphasis=false) {
        tx.draw(top, x, 704, 21, emphasis ? WHITE : ICE_BLUE, emphasis, true);
        if (!bottom.empty()) tx.draw(bottom, x, 735, 13, MUTED, false, true);
    };

    std::string temp = "--";
    if (s.vehicle.outside_c && frameFresh(s, Decoder::ID_AMBIENT_TEMP, now, 5.0)) {
        std::ostringstream ss;
        if (imperial) ss << std::lround(*s.vehicle.outside_c * 9.0 / 5.0 + 32.0) << " F";
        else ss << std::lround(*s.vehicle.outside_c) << " C";
        temp = ss.str();
    }
    field(145, temp, "OUTSIDE");

    std::string dte = "--";
    if (s.vehicle.dte_km && frameFresh(s, Decoder::ID_DTE, now, 5.0)) {
        std::ostringstream ss;
        ss << std::lround(imperial ? *s.vehicle.dte_km * 0.621371 : *s.vehicle.dte_km)
           << (imperial ? " mi" : " km");
        dte = ss.str();
    }
    field(355, dte, "TO E");

    const std::string selected = frameFresh(s, Decoder::ID_PRNDL, now, 2.0) ? gearLetter(s.vehicle.gear) : "-";
    const std::array<std::string,5> gears{{"P","R","N","D","L"}};
    int gx = 548;
    for (const auto& g : gears) {
        const bool on = (selected == g) || (selected == "S" && g == "D");
        // OEM-style PRNDL emphasis: selected range is larger, bold Antenna
        // Regular in Ford orange; inactive ranges stay Antenna Light/dim gray.
        tx.draw(g, gx, 700, on ? 31 : 24, on ? MIL_ORANGE : DARK_MUTED, on, true);
        gx += 46;
    }
    if (selected == "S") tx.draw("S", 751, 739, 15, MIL_ORANGE, true, true);
    tx.draw("PRNDL", 640, 750, 12, MUTED, false, true);

    std::string hdg = compassText(s);
    if (s.vehicle.heading_deg && frameFresh(s, Decoder::ID_GPS_HEADING, now, 6.0)) {
        std::ostringstream ss; ss << hdg << " " << std::lround(*s.vehicle.heading_deg);
        hdg = ss.str();
    } else if (!frameFresh(s, Decoder::ID_GPS_COMPASS, now, 6.0)) hdg = "--";
    field(875, hdg, "HEADING");

    std::string odo = "--";
    if (s.vehicle.odometer_km && frameFresh(s, Decoder::ID_ODOMETER, now, 10.0)) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(0)
           << (imperial ? *s.vehicle.odometer_km * 0.621371 : *s.vehicle.odometer_km)
           << (imperial ? " mi" : " km");
        odo = ss.str();
    }
    field(1100, odo, "ODOMETER");
}


void drawPageDots(SDL_Renderer* r, CenterView view);

void drawCenterScene(SDL_Renderer* r, TextCache& tx, AssetStore& assets,
                     const ADASState& s, double now, const std::string& units) {
    constexpr int X0 = 390;
    constexpr int X1 = 890;
    constexpr int CX = 640;
    constexpr int DIVIDER_TOP = 82;
    constexpr int BOTTOM = 660;

    // The road surface begins where the Ford lane sprites become visible.
    // Their visible inner edges begin near x=612/668 around y=288.  Starting
    // the road at the same points keeps the road sides visually aligned with
    // the Ford lane-line perspective (about 31 degrees vs. ~33 degrees from
    // vertical) while terminating the lower corners exactly at the two center
    // divider lines (390 and 890).
    constexpr int ROAD_TOP = 288;
    constexpr int ROAD_TOP_HALF = 28;
    constexpr int ROAD_BOTTOM_HALF = (X1 - X0) / 2; // 250 px

    for (int y = ROAD_TOP; y <= BOTTOM; ++y) {
        const double p = (y - ROAD_TOP) / static_cast<double>(BOTTOM - ROAD_TOP);
        const int half = static_cast<int>(std::lround(
            ROAD_TOP_HALF + p * (ROAD_BOTTOM_HALF - ROAD_TOP_HALF)));

        // Subtle vertical blue-black gradient preserves road depth without
        // turning the center pane into one oversized flat black mass.
        const Uint8 rr = static_cast<Uint8>(3 + std::lround(3.0 * p));
        const Uint8 gg = static_cast<Uint8>(8 + std::lround(6.0 * p));
        const Uint8 bb = static_cast<Uint8>(12 + std::lround(7.0 * p));
        setColor(r, {rr, gg, bb, 255});
        SDL_RenderDrawLine(r, CX - half, y, CX + half, y);
    }

    // Faint perspective edges visually lock the road surface to the lane lines.
    thickLine(r, CX - ROAD_TOP_HALF, ROAD_TOP, X0, BOTTOM, 1, {21, 55, 70, 150});
    thickLine(r, CX + ROAD_TOP_HALF, ROAD_TOP, X1, BOTTOM, 1, {21, 55, 70, 150});

    // Center-pane dividers remain independent from the road surface.
    thickLine(r, X0, DIVIDER_TOP, X0, BOTTOM, 1, {27, 74, 96, 180});
    thickLine(r, X1, DIVIDER_TOP, X1, BOTTOM, 1, {27, 74, 96, 180});

    const double laneAngle = clampd(s.lane.path_angle_rad * 57.2958 + s.lane.reference_angle_mrad * 0.008, -5.0, 5.0);
    const int pathOffset = static_cast<int>(std::lround(clampd(s.lane.path_offset_m, -1.2, 1.2) * 22.0));
    const double curveShift = clampd(s.lane.curvature, -0.006, 0.006) * 4300.0;
    const int lateral = pathOffset + static_cast<int>(std::lround(curveShift));

    SDL_Rect leftLane{CX - 226 + lateral, 272, 215, 292};
    SDL_Rect rightLane{CX + 10 + lateral, 272, 216, 292};
    const bool laneStale = s.lane.updated <= 0 || now - s.lane.updated > 1.2;
    assets.draw(laneAssetName(true, s.lane, now), leftLane, laneAngle, SDL_FLIP_NONE, laneStale ? 85 : 255);
    assets.draw(laneAssetName(false, s.lane, now), rightLane, laneAngle, SDL_FLIP_NONE, laneStale ? 85 : 255);

    const bool leadFresh = s.lead.valid && s.lead.updated > 0 && now - s.lead.updated < 1.0;
    if (leadFresh) {
        const std::string lead = leadAssetForDistance(s.lead.distance_m);
        const int leadX = CX + static_cast<int>(std::lround(clampd(s.lead.lateral_m, -2.5, 2.5) * 24.0));
        SDL_Rect car{leadX - 92, 214, 184, 88};
        assets.draw(lead, car);
        tx.draw(distanceText(s.lead.distance_m, s.signs.unit, units), CX, 112, 28, WHITE, true, true);
        if (s.lead.rel_long_mps < -0.5) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1) << std::abs(s.lead.rel_long_mps) << " m/s closing";
            tx.draw(ss.str(), CX, 151, 16, (s.lead.ttc_s && *s.lead.ttc_s < 2.5) ? RED : AMBER, false, true);
        }
    } else {
        tx.draw("NO VEHICLE AHEAD", CX, 124, 18, MUTED, false, true);
    }

    if (s.follow_gap_display && s.follow_gap >= 1 && s.follow_gap <= 5) {
        const int gap = std::max(1, std::min(4, s.follow_gap));
        const int h = gap == 1 ? 28 : (gap == 2 ? 42 : (gap == 3 ? 50 : 58));
        SDL_Rect gapDst{CX - 86, 390, 172, h};
        assets.draw(gapAssetForValue(gap, true), gapDst);
    }

    // The current Escape layout intentionally does not place the small host
    // vehicle under the ACC gap bars. The wider GE panel keeps that behavior.
    const bool laneFresh = s.lane.updated > 0 && now - s.lane.updated < 1.2;
    if (!laneFresh) tx.draw("LANE DATA UNAVAILABLE", CX, 612, 17, MUTED, false, true);
    else if (s.lane.departure_state)
        tx.draw("LANE DEPARTURE", CX, 612, 18, RED, true, true);
    else
        tx.draw("LANE KEEPING", CX, 612, 17, MUTED, false, true);
    drawPageDots(r, CenterView::ADAS);
}





void drawTrafficSigns(SDL_Renderer* r, TextCache& tx, AssetStore& assets,
                      const TrafficSigns& signs, double now) {
    (void)r;
    constexpr int CX = 844;
    const bool stale = signs.updated <= 0 || now - signs.updated > 2.0;

    tx.draw("SIGNS", CX, 92, 14, MUTED, true, true);
    SDL_Rect signRect{CX - 43, 117, 86, 86};
    assets.draw("speed_limit_blank", signRect, 0.0, SDL_FLIP_NONE, stale ? 90 : 255);
    const std::string lim = (!stale && signs.speed_limit_1) ? std::to_string(*signs.speed_limit_1) : "--";
    tx.drawSign(lim, CX, 135, lim.size() >= 3 ? 27 : 33, {20, 20, 20, 255}, true);
    tx.drawSign(stale ? "" : speedUnitText(signs.unit), CX, 176, 11, {35, 35, 35, 255}, true);

    if (!stale && signs.overtaking != OvertakingRestriction::None) {
        SDL_Rect noPass{CX - 34, 217, 68, 68};
        assets.draw("no_passing", noPass);
        tx.draw("NO PASSING", CX, 292, 12, WHITE, false, true);
    } else if (!stale && signs.restriction != SignRestriction::None) {
        tx.draw(restrictionText(signs.restriction), CX, 235, 18, AMBER, true, true);
        tx.draw("CONDITION", CX, 262, 12, MUTED, false, true);
    } else {
        tx.draw(stale ? "NO DATA" : "RECOGNITION", CX, 239, 12, stale ? DARK_MUTED : MUTED, false, true);
        tx.draw(stale ? "" : "ACTIVE", CX, 264, 13, stale ? DARK_MUTED : ICE_BLUE, false, true);
    }
}

std::string centerViewName(CenterView v) {
    switch (v) {
        case CenterView::ADAS: return "DRIVER ASSIST";
        case CenterView::TripA: return "TRIP COMPUTER A";
        case CenterView::TripB: return "TRIP COMPUTER B";
        case CenterView::NowPlaying: return "NOW PLAYING";
        case CenterView::Vehicle: return "VEHICLE";
        case CenterView::Count: return "";
    }
    return "";
}

void drawPageDots(SDL_Renderer* r, CenterView view) {
    constexpr int count = static_cast<int>(CenterView::Count);
    constexpr int cx = 640;
    constexpr int y = 652;
    constexpr int spacing = 23;
    const int first = cx - (count - 1) * spacing / 2;
    for (int i = 0; i < count; ++i) {
        const bool active = i == static_cast<int>(view);
        fillCircle(r, first + i * spacing, y, active ? 4 : 3, active ? ICE_BLUE : DARK_MUTED);
    }
}

std::string formatElapsed(double sec) {
    const auto total = std::max<long long>(0, static_cast<long long>(std::llround(sec)));
    const long long h = total / 3600;
    const long long m = (total % 3600) / 60;
    std::ostringstream ss;
    if (h > 0) ss << h << ':' << std::setw(2) << std::setfill('0') << m;
    else ss << m << ':' << std::setw(2) << std::setfill('0') << (total % 60);
    return ss.str();
}

std::string clippedText(std::string_view v, std::size_t maxLen) {
    if (v.empty()) return "--";
    if (v.size() <= maxLen) return std::string(v);
    if (maxLen < 4U) return std::string(v.substr(0U, maxLen));
    return std::string(v.substr(0U, maxLen - 3U)) + "...";
}

const char* doorPopupText(DoorPopupDetail detail) noexcept {
    switch (detail) {
        case DoorPopupDetail::DriverDoor: return "DRIVER DOOR OPEN";
        case DoorPopupDetail::PassengerDoor: return "PASSENGER DOOR OPEN";
        case DoorPopupDetail::LeftRearDoor: return "LEFT REAR DOOR OPEN";
        case DoorPopupDetail::RightRearDoor: return "RIGHT REAR DOOR OPEN";
        case DoorPopupDetail::MultipleDoors: return "MULTIPLE DOORS OPEN";
        case DoorPopupDetail::Generic: default: return "DOOR OPEN";
    }
}

void drawTripComputerView(SDL_Renderer* r, TextCache& tx, const ADASState& s,
                          const TripComputer& trip, bool tripA) {
    constexpr int X0 = 390, X1 = 890, CX = 640;
    fillRect(r, X0, 82, X1 - X0, 578, {1, 8, 14, 120});
    thickLine(r, X0, 82, X0, 660, 1, {27, 74, 96, 255});
    thickLine(r, X1, 82, X1, 660, 1, {27, 74, 96, 255});
    tx.draw(tripA ? "TRIP COMPUTER A" : "TRIP COMPUTER B", CX, 100, 23, WHITE, true, true);

    const bool imperial = useImperial(s);
    const double dist = imperial ? trip.distance_km * 0.621371 : trip.distance_km;
    const double avg = imperial ? trip.averageSpeedKph() * 0.621371 : trip.averageSpeedKph();
    std::ostringstream ds;
    ds << std::fixed << std::setprecision(1) << dist << (imperial ? " mi" : " km");
    tx.draw(ds.str(), CX, 166, 46, WHITE, true, true);
    tx.draw("TRIP DISTANCE", CX, 220, 14, MUTED, false, true);

    tx.draw(formatElapsed(trip.elapsed_s), 485, 315, 31, ICE_BLUE, true, true);
    tx.draw("ELAPSED", 485, 354, 13, MUTED, false, true);

    std::ostringstream av;
    if (trip.elapsed_s > 0.5) av << std::fixed << std::setprecision(1) << avg << (imperial ? " MPH" : " km/h");
    else av << "--";
    tx.draw(av.str(), 795, 315, 31, ICE_BLUE, true, true);
    tx.draw("AVG SPEED", 795, 354, 13, MUTED, false, true);

    tx.draw("--", 485, 445, 31, ICE_BLUE, true, true);
    tx.draw(imperial ? "AVG MPG" : "AVG L/100km", 485, 484, 13, MUTED, false, true);
    tx.draw("OK TO RESET", 795, 447, 18, DARK_MUTED, true, true);
    tx.draw("STEERING-WHEEL OK", 795, 482, 12, MUTED, false, true);
    drawPageDots(r, tripA ? CenterView::TripA : CenterView::TripB);
}


void drawNowPlayingView(SDL_Renderer* r, TextCache& tx, const ADASState& s) {
    constexpr int X0 = 390, X1 = 890, CX = 640;
    fillRect(r, X0, 82, X1 - X0, 578, {1, 8, 14, 120});
    thickLine(r, X0, 82, X0, 660, 1, {27, 74, 96, 255});
    thickLine(r, X1, 82, X1, 660, 1, {27, 74, 96, 255});
    tx.draw("NOW PLAYING", CX, 100, 23, WHITE, true, true);

    tx.draw(clippedText(s.media.title.view(), 34), CX, 180, 36, WHITE, true, true);
    tx.draw(clippedText(s.media.artist.view(), 38), CX, 237, 25, ICE_BLUE, true, true);
    tx.draw(clippedText(s.media.album.view(), 45), CX, 282, 18, MUTED, false, true);

    std::string track = "--";
    if (s.media.track_number && s.media.track_count)
        track = std::to_string(*s.media.track_number) + " / " + std::to_string(*s.media.track_count);
    else if (s.media.track_number) track = std::to_string(*s.media.track_number);
    tx.draw("TRACK " + track, 500, 405, 16, MUTED, true, true);
    tx.draw("DEVICE  " + clippedText(s.media.device.view(), 27), 780, 405, 16, MUTED, true, true);
    if (s.media.title.empty() && s.media.artist.empty())
        tx.draw("MEDIA TEXT SOURCE NOT AVAILABLE", CX, 520, 15, DARK_MUTED, false, true);
    drawPageDots(r, CenterView::NowPlaying);
}


void drawVehicleStatusView(SDL_Renderer* r, TextCache& tx, AssetStore& assets,
                           const ADASState& s, double now,
                           const VehiclePanelVisual& vehicleVisual) {
    constexpr int X0 = 390, X1 = 890, CX = 640;
    fillRect(r, X0, 82, X1 - X0, 578, {1, 8, 14, 120});
    thickLine(r, X0, 82, X0, 660, 1, {27, 74, 96, 255});
    thickLine(r, X1, 82, X1, 660, 1, {27, 74, 96, 255});
    tx.draw("VEHICLE", CX, 100, 23, WHITE, true, true);

    const bool fresh = s.mode_text == "DEMO" || (s.body.updated > 0.0 && now - s.body.updated < 3.0);
    drawIntegratedVehicle(r, assets, 545, 168, 150, 320, vehicleVisual, fresh ? 255 : 95);

    if (fresh) {
        const bool any = s.body.driver_front_ajar || s.body.passenger_front_ajar ||
                         s.body.rear_left_ajar || s.body.rear_right_ajar ||
                         s.body.hood_ajar || s.body.liftgate_ajar;
        tx.draw(any ? "AJAR" : "ALL CLOSED", 760, 188, 27,
                any ? MIL_ORANGE : GREEN, true, true);
        int y = 246;
        if (s.body.hood_ajar) { tx.draw("HOOD", 760, y, 17, MIL_ORANGE, true, true); y += 31; }
        if (s.body.liftgate_ajar) { tx.draw("LIFTGATE", 760, y, 17, MIL_ORANGE, true, true); y += 31; }
        if (s.body.driver_front_ajar) { tx.draw("DRIVER DOOR", 760, y, 17, MIL_ORANGE, true, true); y += 31; }
        if (s.body.passenger_front_ajar) { tx.draw("PASSENGER DOOR", 760, y, 17, MIL_ORANGE, true, true); y += 31; }
        if (s.body.rear_left_ajar) { tx.draw("LEFT REAR DOOR", 760, y, 17, MIL_ORANGE, true, true); y += 31; }
        if (s.body.rear_right_ajar) { tx.draw("RIGHT REAR DOOR", 760, y, 17, MIL_ORANGE, true, true); }
    } else {
        tx.draw("NO BODY STATUS", 760, 286, 19, DARK_MUTED, true, true);
    }
    drawPageDots(r, CenterView::Vehicle);
}


void drawPowerIcon(SDL_Renderer* r, int cx, int cy, Uint8 alpha) {
    const Color c = alphaColor(AMBER, alpha);
    drawCircle(r, cx, cy, 27, 4, c);
    thickLine(r, cx, cy - 36, cx, cy - 6, 6, c);
}


void drawWasherIcon(SDL_Renderer* r, int cx, int cy, Uint8 alpha) {
    const Color c = alphaColor(AMBER, alpha);
    thickLine(r, cx - 36, cy + 12, cx + 36, cy + 12, 4, c);
    thickLine(r, cx - 36, cy + 12, cx - 26, cy - 17, 4, c);
    thickLine(r, cx + 36, cy + 12, cx + 26, cy - 17, 4, c);
    thickLine(r, cx - 26, cy - 17, cx + 26, cy - 17, 4, c);
    thickLine(r, cx - 21, cy - 30, cx - 9, cy - 41, 3, c);
    thickLine(r, cx, cy - 30, cx + 8, cy - 43, 3, c);
    thickLine(r, cx + 20, cy - 29, cx + 29, cy - 40, 3, c);
}


void drawCoolantIcon(SDL_Renderer* r, int cx, int cy, Uint8 alpha, Color base = RED) {
    const Color c = alphaColor(base, alpha);
    drawCircle(r, cx - 14, cy - 15, 8, 3, c);
    thickLine(r, cx - 14, cy - 8, cx - 14, cy + 15, 6, c);
    fillCircle(r, cx - 14, cy + 20, 9, c);
    for (int row = 0; row < 2; ++row) {
        const int y = cy + 28 + row * 10;
        thickLine(r, cx + 3, y, cx + 15, y - 5, 3, c);
        thickLine(r, cx + 15, y - 5, cx + 27, y, 3, c);
        thickLine(r, cx + 27, y, cx + 39, y - 5, 3, c);
    }
}


void drawWrenchIcon(SDL_Renderer* r, int cx, int cy, Uint8 alpha, Color base = AMBER) {
    const Color c = alphaColor(base, alpha);
    thickLine(r, cx - 23, cy + 24, cx + 12, cy - 11, 10, c);
    drawCircle(r, cx - 26, cy + 27, 9, 4, c);
    thickLine(r, cx + 8, cy - 15, cx + 24, cy - 31, 7, c);
    thickLine(r, cx + 8, cy - 15, cx + 28, cy - 3, 7, c);
}


void drawPopupOk(SDL_Renderer* r, TextCache& tx, Uint8 alpha,
                 int panelX, int panelY, int panelW, int panelH, Color accent) {
    constexpr int W = 92;
    constexpr int H = 42;
    constexpr int Margin = 20;
    const int x = panelX + panelW - W - Margin;
    const int y = panelY + panelH - H - Margin;
    const Color edge = alphaColor(accent, alpha);
    const Color fill = alphaColor(Color{7, 26, 35, 235}, alpha);
    fillRect(r, x, y, W, H, fill);
    drawRect(r, x, y, W, H, edge);
    tx.draw("OK", x + W / 2, y + 7, 20, alphaColor(WHITE, alpha), true, true);
}


void drawPopupOverlay(SDL_Renderer* r, TextCache& tx, AssetStore& assets,
                      const PopupVisual& p) {
    if (!p.active || p.alpha == 0) return;
    const Uint8 a = p.alpha;
    constexpr int PX = 250;
    constexpr int PY = 166;
    constexpr int PW = 780;
    constexpr int PH = 432;

    const bool red = p.event.kind == PopupKind::DoorAjar ||
                     p.event.kind == PopupKind::Powertrain ||
                     p.event.kind == PopupKind::EngineOverTemperature ||
                     p.event.kind == PopupKind::EngineOilPressureLow ||
                     p.event.kind == PopupKind::ChargingSystem;
    const Color accent = red ? RED : AMBER;
    const Color panel = alphaColor(red ? Color{18, 4, 7, 244} : Color{13, 10, 2, 244}, a);
    const Color inner = alphaColor(red ? Color{67, 12, 18, 178} : Color{72, 51, 8, 178}, a);

    fillRect(r, PX, PY, PW, PH, panel);
    fillRect(r, PX, PY, 9, PH, alphaColor(accent, a));
    drawRect(r, PX, PY, PW, PH, alphaColor(accent, a));
    drawRect(r, PX + 6, PY + 6, PW - 12, PH - 12, inner);

    if (p.event.kind == PopupKind::DoorAjar) {
        tx.draw("DOOR AJAR", 640, 186, 27, alphaColor(RED, a), true, true);
        const double move = smooth01(clampd(p.age / 0.62, 0.0, 1.0));
        const auto vv = interpolatePanels(p.event.body_before, p.event.body_after, move);
        drawIntegratedVehicle(r, assets, 475, 236, 145, 310, vv, a);
        tx.draw(doorPopupText(p.event.door_detail), 748, 294, 24, alphaColor(RED, a), true, true);
        tx.draw("CHECK DOORS", 748, 344, 18, alphaColor(WHITE, a), false, true);
        tx.draw("OPEN PANEL SHOWN", 748, 382, 14, alphaColor(MUTED, a), false, true);
    } else {
        const int iconX = 420;
        const int iconY = 360;
        std::string line1;
        switch (p.event.kind) {
            case PopupKind::AccessoryPower:
                drawPowerIcon(r, iconX, iconY, a);
                line1 = "FULL ACCESSORY POWER ENABLED";
                break;
            case PopupKind::WasherFluid:
                drawWasherIcon(r, iconX, iconY, a);
                line1 = "WASHER FLUID LEVEL LOW";
                break;
            case PopupKind::SeeManual:
                drawWrenchIcon(r, iconX, iconY, a, AMBER);
                line1 = "SEE MANUAL";
                break;
            case PopupKind::Powertrain:
                drawWrenchIcon(r, iconX, iconY, a, RED);
                line1 = "POWERTRAIN MALFUNCTION";
                break;
            case PopupKind::EngineOverTemperature:
                drawCoolantIcon(r, iconX, iconY, a);
                line1 = "ENGINE OVER TEMPERATURE";
                break;
            case PopupKind::EngineOilPressureLow:
                assets.draw("telltale_oil", SDL_Rect{iconX - 39, iconY - 24, 78, 48},
                            0.0, SDL_FLIP_NONE, a);
                line1 = "ENGINE OIL PRESSURE LOW";
                break;
            case PopupKind::ChargingSystem:
                assets.draw("telltale_battery", SDL_Rect{iconX - 38, iconY - 31, 76, 62},
                            0.0, SDL_FLIP_NONE, a);
                line1 = "CHARGING SYSTEM MALFUNCTION";
                break;
            case PopupKind::DoorAjar:
                break;
        }
        tx.draw(line1, 708, 332, 24, alphaColor(accent, a), true, true);
    }
    drawPopupOk(r, tx, a, PX, PY, PW, PH, accent);
}


void drawSelectableCenter(SDL_Renderer* r, TextCache& tx, AssetStore& assets,
                          const ADASState& s, const TripManager& trips,
                          double now, const std::string& units, CenterView view,
                          const VehiclePanelVisual& vehicleVisual) {
    switch (view) {
        case CenterView::ADAS:
            drawCenterScene(r, tx, assets, s, now, units);
            drawTrafficSigns(r, tx, assets, s.signs, now);
            break;
        case CenterView::TripA:
            drawTripComputerView(r, tx, s, trips.a(), true);
            break;
        case CenterView::TripB:
            drawTripComputerView(r, tx, s, trips.b(), false);
            break;
        case CenterView::NowPlaying:
            drawNowPlayingView(r, tx, s);
            break;
        case CenterView::Vehicle:
            drawVehicleStatusView(r, tx, assets, s, now, vehicleVisual);
            break;
        case CenterView::Count:
            break;
    }
}

void drawIntegrityOverlay(SDL_Renderer* r, TextCache& tx, const ADASState& s) {
    if (s.integrity_state == IntegrityState::Nominal) return;
    if (s.integrity_state == IntegrityState::Degraded) {
        fillRect(r, 500, 70, 280, 38, {55, 38, 0, 235});
        drawRect(r, 500, 70, 280, 38, AMBER);
        tx.draw("DATA INTEGRITY DEGRADED", 640, 77, 16, AMBER, true, true);
        return;
    }

    constexpr int X = 250, Y = 204, W = 780, H = 340;
    fillRect(r, X, Y, W, H, {24, 2, 5, 250});
    fillRect(r, X, Y, 10, H, RED);
    drawRect(r, X, Y, W, H, RED);
    tx.draw("VEHICLE DATA UNAVAILABLE", X + W / 2, Y + 58, 31, RED, true, true);
    tx.draw("DO NOT RELY ON DISPLAYED DYNAMIC VALUES", X + W / 2, Y + 128, 19, WHITE, true, true);
    tx.draw("CHECK CAN / DISPLAY SYSTEM", X + W / 2, Y + 177, 19, WHITE, false, true);
    std::ostringstream code;
    code << "FAULT 0x" << std::hex << std::uppercase << s.integrity_faults;
    tx.draw(code.str(), X + W / 2, Y + 238, 16, MUTED, false, true);
}


void renderScene(SDL_Renderer* r, SDL_Texture* sceneTarget, SDL_Texture* pageLayer,
                 TextCache& tx, AssetStore& assets, const ADASState& s,
                 const TripManager& trips, bool debug, const std::string& units,
                 const StartupPostVisual& post, const PageTransitionVisual& page,
                 const VehiclePanelVisual& vehicleVisual, const PopupVisual& popup,
                 double rasterScale) {
    const double now = monotonicSeconds();
    setColor(r, BLACK);
    SDL_RenderClear(r);

    // Preserve the genuine Ford blue texture but use the original GE strategy:
    // stretch it to the 1280x800 panel and darken the top/bottom zones so the
    // expanded vertical composition retains the OEM cluster character.
    assets.draw("ipc_background", SDL_Rect{0, 0, LogicalWidth, LogicalHeight});
    fillRect(r, 0, 0, LogicalWidth, 76, {0, 0, 0, 78});
    fillRect(r, 0, 660, LogicalWidth, 140, {0, 0, 0, 82});

    const bool imperial = useImperial(s);
    const bool speedFresh = s.vehicle.speed_kph.has_value() && frameFresh(s, Decoder::ID_VEHICLE_SPEED, now, 1.0);
    const bool rpmFresh = s.vehicle.rpm.has_value() && frameFresh(s, Decoder::ID_ENGINE_RPM, now, 1.0);
    const double speed = speedFresh ? (imperial ? *s.vehicle.speed_kph * 0.621371 : *s.vehicle.speed_kph) : 0.0;
    const double maxSpeed = imperial ? 160.0 : 240.0;
    const double liveRpm = rpmFresh ? *s.vehicle.rpm : 0.0;
    const double speedNeedle = post.active ? post.sweep_fraction * maxSpeed : speed;
    const double rpmNeedle = post.active ? post.sweep_fraction * 8000.0 : liveRpm;
    const double speedDisplay = post.active ? 0.0 : speed;
    const double rpmDisplay = post.active ? 0.0 : liveRpm;
    const bool speedGaugeValid = post.active || speedFresh;
    const bool rpmGaugeValid = post.active || rpmFresh;

    drawGauge(r, tx, assets, 190, 304, 164, speedNeedle, speedDisplay, speedGaugeValid, maxSpeed,
              "SPEED", imperial ? "MPH" : "km/h", imperial ? 20 : 30, false);
    drawGauge(r, tx, assets, 1090, 304, 164, rpmNeedle, rpmDisplay, rpmGaugeValid, 8000.0,
              "RPM", "x1000", 1000, true);

    SDL_SetRenderTarget(r, pageLayer);
    SDL_RenderSetScale(r, static_cast<float>(rasterScale), static_cast<float>(rasterScale));
    setColor(r, {0, 0, 0, 0});
    SDL_RenderClear(r);
    drawSelectableCenter(r, tx, assets, s, trips, now, units, page.view, vehicleVisual);
    SDL_SetRenderTarget(r, sceneTarget);
    SDL_RenderSetScale(r, static_cast<float>(rasterScale), static_cast<float>(rasterScale));
    SDL_SetTextureAlphaMod(pageLayer, page.alpha);
    float sx = 1.0F, sy = 1.0F;
    SDL_RenderGetScale(r, &sx, &sy);
    SDL_RenderSetScale(r, 1.0F, 1.0F);
    int sw = 0, sh = 0;
    SDL_QueryTexture(sceneTarget, nullptr, nullptr, &sw, &sh);
    SDL_Rect fullTarget{0, 0, sw, sh};
    SDL_RenderCopy(r, pageLayer, nullptr, &fullTarget);
    SDL_RenderSetScale(r, sx, sy);
    SDL_SetTextureAlphaMod(pageLayer, 255);

    drawCompactSideStatus(r, tx, assets, s.left, true, now);
    drawCompactSideStatus(r, tx, assets, s.right, false, now);
    if (post.active) drawPostTelltales(assets, post.telltale_alpha);
    else drawTelltales(r, assets, s, now);
    drawStatusBand(r, tx, s, now);
    if (!post.active) drawPopupOverlay(r, tx, assets, popup);
    drawIntegrityOverlay(r, tx, s);

    if (debug) {
        fillRect(r, 430, 112, 420, 72, {0, 0, 0, 220});
        std::ostringstream ss;
        ss << s.mode_text << "   " << std::fixed << std::setprecision(0) << s.frames_per_sec << " CAN fps";
        tx.draw(ss.str(), 640, 122, 16, ICE_BLUE, true, true);
        tx.draw("frames " + std::to_string(s.frame_count), 640, 149, 13, MUTED, false, true);
        if (!s.last_error.empty()) tx.draw(std::string(s.last_error.view()), 640, 168, 12, RED, false, true);
    }
}



void presentScene(SDL_Renderer* r, SDL_Texture* scene, int rotation) {
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(r, &ow, &oh);
    setColor(r, BLACK);
    SDL_RenderClear(r);
    const bool quarter = rotation == 90 || rotation == 270;
    const double bw = quarter ? LogicalHeight : LogicalWidth;
    const double bh = quarter ? LogicalWidth : LogicalHeight;
    const double scale = std::min(ow / bw, oh / bh);
    const int prew = (int)std::lround(LogicalWidth * scale);
    const int preh = (int)std::lround(LogicalHeight * scale);
    SDL_Rect dst{ow / 2 - prew / 2, oh / 2 - preh / 2, prew, preh};
    SDL_RenderCopyEx(r, scene, nullptr, &dst, (double)rotation, nullptr, SDL_FLIP_NONE);
    SDL_RenderPresent(r);
}



} // namespace

struct ClusterUi::Impl {
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* scene = nullptr;
    SDL_Texture* page_layer = nullptr;
    double raster_scale = 1.0;
    std::unique_ptr<AssetStore> assets;
    std::unique_ptr<TextCache> text;

    Impl(SDL_Renderer* r, const std::filesystem::path& assetPath,
         const std::filesystem::path& fontPath, double rasterScale, int sceneW, int sceneH)
        : renderer(r), raster_scale(rasterScale) {
        scene = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, sceneW, sceneH);
        if (!scene) throw std::runtime_error("render-target texture unavailable: " + std::string(SDL_GetError()));
        SDL_SetTextureBlendMode(scene, SDL_BLENDMODE_BLEND);
        page_layer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, sceneW, sceneH);
        if (!page_layer) { SDL_DestroyTexture(scene); scene=nullptr; throw std::runtime_error("page-layer texture unavailable: " + std::string(SDL_GetError())); }
        SDL_SetTextureBlendMode(page_layer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        assets = std::make_unique<AssetStore>(renderer, assetPath);
        text = std::make_unique<TextCache>(renderer, fontPath, rasterScale);
    }

    ~Impl() {
        text.reset();
        assets.reset();
        if (page_layer) SDL_DestroyTexture(page_layer);
        if (scene) SDL_DestroyTexture(scene);
    }
};

ClusterUi::ClusterUi(SDL_Renderer* renderer, const std::filesystem::path& assetPath,
                     const std::filesystem::path& fontPath, double rasterScale,
                     int sceneWidth, int sceneHeight)
    : impl_(std::make_unique<Impl>(renderer, assetPath, fontPath, rasterScale, sceneWidth, sceneHeight)) {}

ClusterUi::~ClusterUi() = default;

void ClusterUi::renderFrame(const ADASState& state, const TripManager& trips, bool debug,
                            const std::string& distanceUnits, const StartupPostVisual& post,
                            const PageTransitionVisual& page, const VehiclePanelVisual& vehicleVisual,
                            const PopupVisual& popup, int rotation) {
    SDL_SetRenderTarget(impl_->renderer, impl_->scene);
    SDL_RenderSetScale(impl_->renderer, static_cast<float>(impl_->raster_scale), static_cast<float>(impl_->raster_scale));
    renderScene(impl_->renderer, impl_->scene, impl_->page_layer, *impl_->text, *impl_->assets,
                state, trips, debug, distanceUnits, post, page, vehicleVisual, popup, impl_->raster_scale);
    SDL_RenderSetScale(impl_->renderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(impl_->renderer, nullptr);
    presentScene(impl_->renderer, impl_->scene, rotation);
}


void ClusterUi::renderEmergencyFrame(int rotation) noexcept {
    if (!impl_ || !impl_->renderer || !impl_->scene) return;
    SDL_SetRenderTarget(impl_->renderer, impl_->scene);
    SDL_RenderSetScale(impl_->renderer, static_cast<float>(impl_->raster_scale), static_cast<float>(impl_->raster_scale));
    setColor(impl_->renderer, {20, 0, 0, 255});
    SDL_RenderClear(impl_->renderer);

    // High-contrast no-font fallback: red field, white frame and crossed center.
    // This remains recognizable even when font/asset allocation failed.
    drawRect(impl_->renderer, 18, 18, LogicalWidth - 36, LogicalHeight - 36, RED);
    fillRect(impl_->renderer, 300, 220, 680, 360, {5, 5, 5, 245});
    drawRect(impl_->renderer, 300, 220, 680, 360, WHITE);
    thickLine(impl_->renderer, 445, 295, 835, 505, 13, RED);
    thickLine(impl_->renderer, 835, 295, 445, 505, 13, RED);

    SDL_RenderSetScale(impl_->renderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(impl_->renderer, nullptr);
    presentScene(impl_->renderer, impl_->scene, rotation);
}

const std::filesystem::path& ClusterUi::assetRoot() const noexcept { return impl_->assets->root(); }

} // namespace fordadas
