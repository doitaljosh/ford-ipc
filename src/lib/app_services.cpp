#include "ford_ipc/app_services.hpp"
#include "ford_ipc/time.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <string>

namespace fordadas {
namespace {
std::string trim(std::string v) {
    const auto b = v.find_first_not_of(" \t\r\n");
    const auto e = v.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? std::string{} : v.substr(b, e - b + 1U);
}
}

MediaFileWatcher::MediaFileWatcher(std::string path) : path_(std::move(path)) {}

void MediaFileWatcher::poll(SharedState& shared, double now) {
    if (path_.empty() || now < next_poll_) return;
    next_poll_ = now + 0.75;
    std::ifstream in(path_);
    if (!in) return;

    std::string title, artist, album, device;
    bool hasTitle=false, hasArtist=false, hasAlbum=false, hasDevice=false;
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0U, eq));
        const std::string value = trim(line.substr(eq + 1U));
        if (key == "title") { title=value; hasTitle=true; }
        else if (key == "artist") { artist=value; hasArtist=true; }
        else if (key == "album") { album=value; hasAlbum=true; }
        else if (key == "device") { device=value; hasDevice=true; }
    }
    (void)shared.tryMutate([&](ADASState& s) {
        if (hasTitle) s.media.title = title;
        if (hasArtist) s.media.artist = artist;
        if (hasAlbum) s.media.album = album;
        if (hasDevice) s.media.device = device;
        s.media.updated = now;
    });
}

void initializeStandbyState(SharedState& shared) {
    shared.mutate([](ADASState& s) {
        s.mode_text = "STANDBY";
        s.vehicle.speed_kph = 0.0;
        s.vehicle.rpm = 0.0;
        s.vehicle.gear = 0;
        s.vehicle.dte_km.reset();
        s.vehicle.outside_c.reset();
        s.vehicle.odometer_km.reset();
        s.vehicle.heading_deg.reset();
        s.vehicle.compass_dir.reset();
        s.signs.unit = SpeedUnit::Unknown;
    });
}

void updateDemo(SharedState& shared, double t) {
    const double phase = std::fmod(t, 30.0);
    shared.mutate([&](ADASState& s) {
        s.mode_text = "DEMO";
        s.frames_per_sec = 75.0;
        s.frame_count += 5U;
        s.last_frame_time = monotonicSeconds();

        s.lane.curvature = 0.0004 * std::sin(t / 5.5);
        s.lane.reference_angle_mrad = 5.0 * std::sin(t / 6.0);
        s.lane.path_offset_m = 0.15 * std::sin(t / 3.8);
        s.lane.path_angle_rad = 0.006 * std::sin(t / 4.3);
        s.lane.departure_state = (phase > 8.0 && phase < 10.0) ? 2 : ((phase > 22.0 && phase < 24.0) ? 4 : 0);
        s.lane.departure_intensity = s.lane.departure_state ? 2 : 0;
        s.lane.lka_state = 2;
        s.lane.updated = monotonicSeconds();

        s.lead.valid = true;
        s.lead.distance_m = 34.0 + 18.0 * std::sin(t / 4.2);
        s.lead.lateral_m = 0.25 * std::sin(t / 3.2);
        s.lead.rel_long_mps = -1.7 + 1.0 * std::sin(t / 2.8);
        s.lead.ttc_s = s.lead.rel_long_mps < -0.1
            ? std::optional<double>(std::max(1.2, s.lead.distance_m / std::max(0.5, -s.lead.rel_long_mps)))
            : std::nullopt;
        s.lead.confidence = 3;
        s.lead.obj_class = 1;
        s.lead.updated = monotonicSeconds();

        s.signs.speed_limit_1 = phase < 15.0 ? 45 : 55;
        s.signs.unit = SpeedUnit::Mph;
        s.signs.restriction = (phase > 17.0 && phase < 23.0) ? SignRestriction::Wet : SignRestriction::None;
        s.signs.overtaking = (phase > 3.0 && phase < 10.0) ? OvertakingRestriction::NoPassing : OvertakingRestriction::None;
        s.signs.tsr_status = 3;
        s.signs.updated = monotonicSeconds();

        s.left.blind_detect = (phase > 4.0 && phase < 11.0) ? 1 : 0;
        s.left.blind_alert = (phase > 6.0 && phase < 9.0) ? 1 : 0;
        s.left.cta_alert = phase > 25.0 && phase < 28.0;
        s.left.updated = monotonicSeconds();

        s.right.blind_detect = (phase > 13.0 && phase < 19.0) ? 1 : 0;
        s.right.blind_alert = (phase > 15.0 && phase < 18.0) ? 1 : 0;
        s.right.cta_alert = phase > 20.0 && phase < 23.0;
        s.right.updated = monotonicSeconds();

        s.follow_mode = true;
        s.follow_gap_display = true;
        s.follow_gap = 3;

        s.body.driver_front_ajar = phase > 21.0 && phase < 24.0;
        s.body.passenger_front_ajar = phase > 24.0 && phase < 27.0;
        s.body.rear_left_ajar = phase > 27.0 && phase < 29.0;
        s.body.rear_right_ajar = false;
        s.body.hood_ajar = phase > 18.0 && phase < 20.0;
        s.body.liftgate_ajar = phase > 20.0 && phase < 22.0;
        s.body.updated = monotonicSeconds();

        s.media.title = "Pull The Plug";
        s.media.artist = "VOILA";
        s.media.album = "Demo Media";
        s.media.device = "SYNC Bluetooth";
        s.media.track_number = 4U;
        s.media.track_count = 12U;
        s.media.updated = monotonicSeconds();

        s.vehicle.speed_kph = 104.6 + 7.0 * std::sin(t / 6.0);
        s.vehicle.rpm = 2050.0 + 450.0 * std::sin(t / 3.2);
        s.vehicle.gear = 3;
        s.vehicle.dte_km = 386.0 - std::fmod(t * 0.04, 25.0);
        s.vehicle.outside_c = 27.2;
        s.vehicle.odometer_km = 135389.4 + t * 0.002;
        s.vehicle.heading_deg = std::fmod(307.0 + t * 0.5, 360.0);
        s.vehicle.compass_dir = 7;

        s.telltales.mil = (phase > 1.0 && phase < 5.0) ? 1 : 0;
        s.telltales.abs_lamp = (phase > 5.5 && phase < 9.0) ? 1 : 0;
        s.telltales.abs_fault = s.telltales.abs_lamp != 0;
        s.telltales.traction_lamp = (phase > 9.5 && phase < 13.0) ? 2 : 0;
        s.telltales.oil_pressure = phase > 13.5 && phase < 16.5;
        s.telltales.brake_fluid = (phase > 17.0 && phase < 20.0) ? 1 : 0;
    });
}

} // namespace fordadas
