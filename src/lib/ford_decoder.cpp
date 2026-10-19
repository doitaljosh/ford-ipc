#include "ford_ipc/ford_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fordadas {

namespace {
constexpr std::array<std::uint32_t, Decoder::SupportedIdCount> kSupportedIds{{
    Decoder::ID_ACCDATA_3, Decoder::ID_SIDE_LEFT, Decoder::ID_SIDE_RIGHT,
    Decoder::ID_LANE_ASSIST, Decoder::ID_TRAFFIC_RECOG, Decoder::ID_LATERAL_MOTION,
    Decoder::ID_STEER_ASSIST, Decoder::ID_ODOMETER, Decoder::ID_PRNDL,
    Decoder::ID_AMBIENT_TEMP, Decoder::ID_VEHICLE_SPEED, Decoder::ID_ENGINE_RPM,
    Decoder::ID_BRAKE_STATUS, Decoder::ID_ABS_STATUS, Decoder::ID_ENGINE_LAMPS,
    Decoder::ID_DTE, Decoder::ID_GPS_COMPASS, Decoder::ID_GPS_HEADING,
    Decoder::ID_STEERING_WHEEL, Decoder::ID_STEERING_DATA, Decoder::ID_BODY_INFO,
    Decoder::ID_APIM_TRACK, Decoder::ID_ENGINE_SERVICE, Decoder::ID_LOW_VOLTAGE_PWR
}};
}

const std::array<std::uint32_t, Decoder::SupportedIdCount>& Decoder::supportedIds() noexcept { return kSupportedIds; }

bool Decoder::isSupportedId(std::uint32_t id) noexcept {
    return std::find(kSupportedIds.begin(), kSupportedIds.end(), id) != kSupportedIds.end();
}

DecodeResult Decoder::feed(std::uint32_t can_id, const std::uint8_t* data, std::size_t len, double now) {
    if (!isSupportedId(can_id)) return DecodeResult::UnsupportedId;
    if (data == nullptr || len != 8U) return DecodeResult::InvalidLength;
    if (!std::isfinite(now) || now < 0.0) return DecodeResult::InvalidTimestamp;

    std::array<std::uint8_t, 8> d{};
    std::copy_n(data, d.size(), d.begin());
    DecodeResult result = DecodeResult::Accepted;

    const bool state_ok = shared_.tryMutate([&](ADASState& s) {
        if (s.frame_count != std::numeric_limits<std::uint64_t>::max()) ++s.frame_count;
        s.last_frame_time = now;
        if (!s.freshness.mark(can_id, now)) result = DecodeResult::FreshnessTableFull;
        if (rate_frames_ != std::numeric_limits<std::uint64_t>::max()) ++rate_frames_;
        if (rate_window_start_ <= 0.0) rate_window_start_ = now;
        const double elapsed = now - rate_window_start_;
        if (elapsed >= 1.0) {
            s.frames_per_sec = static_cast<double>(rate_frames_) / std::max(elapsed, 1e-6);
            rate_frames_ = 0U;
            rate_window_start_ = now;
        }

        if (can_id == ID_LANE_ASSIST) lane(s, d, now);
        else if (can_id == ID_LATERAL_MOTION) lateralMotion(s, d, now);
        else if (can_id == ID_STEER_ASSIST) lead(s, d, now);
        else if (can_id == ID_TRAFFIC_RECOG) traffic(s, d, now);
        else if (can_id == ID_SIDE_LEFT) side(s.left, d, now);
        else if (can_id == ID_SIDE_RIGHT) side(s.right, d, now);
        else if (can_id == ID_ACCDATA_3) acc(s, d);
        else if (can_id == ID_VEHICLE_SPEED) vehicleSpeed(s, d);
        else if (can_id == ID_ENGINE_RPM) engineRpm(s, d);
        else if (can_id == ID_PRNDL) prndl(s, d);
        else if (can_id == ID_DTE) dte(s, d);
        else if (can_id == ID_AMBIENT_TEMP) ambientTemp(s, d);
        else if (can_id == ID_ODOMETER) odometer(s, d);
        else if (can_id == ID_GPS_HEADING) gpsHeading(s, d);
        else if (can_id == ID_GPS_COMPASS) gpsCompass(s, d);
        else if (can_id == ID_ENGINE_LAMPS) engineLamps(s, d);
        else if (can_id == ID_ABS_STATUS) absStatus(s, d);
        else if (can_id == ID_BRAKE_STATUS) brakeStatus(s, d);
        else if (can_id == ID_STEERING_WHEEL) steeringWheel(s, d);
        else if (can_id == ID_STEERING_DATA) gapButtons(s, d);
        else if (can_id == ID_BODY_INFO) bodyInfo(s, d, now);
        else if (can_id == ID_APIM_TRACK) apimTrack(s, d, now);
        else if (can_id == ID_ENGINE_SERVICE) engineService(s, d, now);
        else if (can_id == ID_LOW_VOLTAGE_PWR) lowVoltagePower(s, d, now);
    });
    if (!state_ok) return DecodeResult::StateAccessFailure;
    return result;
}

void Decoder::lane(ADASState& s, const std::array<uint8_t,8>& d, double t) {
    const auto raw_curv = be_raw(d.data(), d.size(), 15, 12);
    const auto raw_ang = be_raw(d.data(), d.size(), 19, 12);
    if (raw_curv != 4095) s.lane.curvature = raw_curv * 5e-6 - 0.01024;
    if (raw_ang != 4095) s.lane.reference_angle_mrad = raw_ang * 0.05 - 102.4;
    s.lane.lka_state = static_cast<int>(be_raw(d.data(), d.size(), 7, 3));
    s.lane.departure_state = static_cast<int>(be_raw(d.data(), d.size(), 4, 3));
    s.lane.departure_intensity = static_cast<int>(be_raw(d.data(), d.size(), 1, 2));
    s.lane.updated = t;
}

void Decoder::lateralMotion(ADASState& s, const std::array<uint8_t,8>& d, double t) {
    s.lane.path_offset_m = be_phys(d.data(), d.size(), 47, 10, 0.01, -5.12);
    s.lane.path_angle_rad = be_phys(d.data(), d.size(), 31, 11, 0.0005, -0.5);
    s.lane.path_curvature = be_phys(d.data(), d.size(), 7, 11, 2e-5, -0.02);
    s.lane.updated = std::max(s.lane.updated, t);
}

void Decoder::lead(ADASState& s, const std::array<uint8_t,8>& d, double t) {
    const auto raw_long = be_raw(d.data(), d.size(), 7, 10);
    const auto raw_conf = be_raw(d.data(), d.size(), 9, 2);
    const auto raw_cls = be_raw(d.data(), d.size(), 13, 4);
    const auto raw_lat = be_raw(d.data(), d.size(), 45, 9);
    const auto raw_rel_lon = be_raw(d.data(), d.size(), 39, 10);
    const auto raw_rel_lat = be_raw(d.data(), d.size(), 23, 9);
    const auto raw_ttc = be_raw(d.data(), d.size(), 30, 7);

    const bool invalid = raw_long >= 1022 || raw_lat >= 510 || raw_rel_lon >= 1022 ||
                         raw_rel_lat >= 510 || raw_ttc >= 126;
    s.lead.valid = !invalid && raw_conf > 0 && raw_long > 0;
    s.lead.distance_m = raw_long * 0.1;
    s.lead.lateral_m = raw_lat * 0.1 - 25.5;
    s.lead.rel_long_mps = raw_rel_lon * 0.1 - 102.1;
    s.lead.rel_lat_mps = raw_rel_lat * 0.1 - 25.5;
    s.lead.ttc_s = raw_ttc >= 126 ? std::nullopt : std::optional<double>(raw_ttc * 0.05);
    s.lead.confidence = static_cast<int>(raw_conf);
    s.lead.obj_class = static_cast<int>(raw_cls);
    s.lead.updated = t;
}

void Decoder::traffic(ADASState& s, const std::array<uint8_t,8>& d, double t) {
    const int lim1 = static_cast<int>(be_raw(d.data(), d.size(), 31, 8));
    const int lim2 = static_cast<int>(be_raw(d.data(), d.size(), 39, 8));
    const int unit = static_cast<int>(be_raw(d.data(), d.size(), 43, 2));
    const int restriction = static_cast<int>(be_raw(d.data(), d.size(), 51, 3));
    const int overtake = static_cast<int>(be_raw(d.data(), d.size(), 55, 4));
    s.signs.tsr_status = static_cast<int>(be_raw(d.data(), d.size(), 10, 3));
    s.signs.osw_warning = be_raw(d.data(), d.size(), 3, 2) == 2;
    s.signs.speed_limit_1 = (lim1 >= 1 && lim1 <= 250) ? std::optional<int>(lim1) : std::nullopt;
    s.signs.speed_limit_2 = (lim2 >= 1 && lim2 <= 250) ? std::optional<int>(lim2) : std::nullopt;
    s.signs.unit = unit == 1 ? SpeedUnit::Kph : unit == 2 ? SpeedUnit::Mph : SpeedUnit::Unknown;
    switch (restriction) {
        case 3: s.signs.restriction = SignRestriction::Wet; break;
        case 4: s.signs.restriction = SignRestriction::Snow; break;
        case 5: s.signs.restriction = SignRestriction::Trailer; break;
        case 6: s.signs.restriction = SignRestriction::Time; break;
        default: s.signs.restriction = SignRestriction::None; break;
    }
    switch (overtake) {
        case 1:
        case 2: s.signs.overtaking = OvertakingRestriction::NoPassing; break;
        case 3: s.signs.overtaking = OvertakingRestriction::NoPassingWet; break;
        case 4: s.signs.overtaking = OvertakingRestriction::NoPassingSnow; break;
        case 5: s.signs.overtaking = OvertakingRestriction::NoPassingTrailer; break;
        case 6: s.signs.overtaking = OvertakingRestriction::NoPassingTime; break;
        default: s.signs.overtaking = OvertakingRestriction::None; break;
    }
    s.signs.updated = t;
}

void Decoder::side(SideStatus& side, const std::array<uint8_t,8>& d, double t) {
    side.blind_alert = static_cast<int>(be_raw(d.data(), d.size(), 10, 2));
    side.blind_sensor = static_cast<int>(be_raw(d.data(), d.size(), 12, 2));
    side.blind_system = static_cast<int>(be_raw(d.data(), d.size(), 15, 3));
    side.cta_system = static_cast<int>(be_raw(d.data(), d.size(), 18, 3));
    side.blind_detect = static_cast<int>(be_raw(d.data(), d.size(), 23, 3));
    side.cta_sensor = static_cast<int>(be_raw(d.data(), d.size(), 25, 2));
    side.cta_alert_level = static_cast<int>(be_raw(d.data(), d.size(), 30, 3));
    side.cta_alert = be_raw(d.data(), d.size(), 8, 1) == 1 || side.cta_alert_level > 0;
    side.updated = t;
}

void Decoder::vehicleSpeed(ADASState& s, const std::array<uint8_t,8>& d) {
    const auto qf = be_raw(d.data(), d.size(), 38, 2);
    const auto raw = be_raw(d.data(), d.size(), 55, 16);
    if (qf == 3 && raw != 0xFFFF && raw <= 30000U) s.vehicle.speed_kph = raw * 0.01;
    else s.vehicle.speed_kph.reset();
}

void Decoder::engineRpm(ADASState& s, const std::array<uint8_t,8>& d) {
    // Gas-engine signal used on the 2020 Escape: EngAout_N_Actl.
    const auto raw = be_raw(d.data(), d.size(), 28, 13);
    if (raw < 8191 && raw <= 5000U) s.vehicle.rpm = raw * 2.0;
    else s.vehicle.rpm.reset();
}

void Decoder::prndl(ADASState& s, const std::array<uint8_t,8>& d) {
    const int raw = static_cast<int>(be_raw(d.data(), d.size(), 7, 4));
    if (raw <= 11) s.vehicle.gear = raw;
    else s.vehicle.gear.reset();
}

void Decoder::dte(ADASState& s, const std::array<uint8_t,8>& d) {
    const auto raw = be_raw(d.data(), d.size(), 39, 14);
    if (raw < 0x3FFE && raw <= 20000U) s.vehicle.dte_km = raw * 0.1;
    else s.vehicle.dte_km.reset();
}

void Decoder::ambientTemp(ADASState& s, const std::array<uint8_t,8>& d) {
    const auto raw = be_raw(d.data(), d.size(), 49, 10);
    const auto qf = be_raw(d.data(), d.size(), 35, 2);
    if (qf == 3 && raw < 0x3FF && raw >= 272U && raw <= 912U) s.vehicle.outside_c = raw * 0.25 - 128.0;
    else if (raw < 0x3FF && raw >= 272U && raw <= 912U) s.vehicle.outside_c = raw * 0.25 - 128.0; // -60..100 C plausibility window
    else s.vehicle.outside_c.reset();
}

void Decoder::odometer(ADASState& s, const std::array<uint8_t,8>& d) {
    const auto raw = be_raw(d.data(), d.size(), 31, 24);
    if (raw != 0xFFFFFF && raw <= 20000000U) s.vehicle.odometer_km = raw * 0.1;
    else s.vehicle.odometer_km.reset();
}

void Decoder::gpsHeading(ADASState& s, const std::array<uint8_t,8>& d) {
    const auto raw = be_raw(d.data(), d.size(), 31, 16);
    if (raw < 0xFFFE) {
        const double deg = raw * 0.01;
        if (deg <= 360.0) s.vehicle.heading_deg = deg;
        else s.vehicle.heading_deg.reset();
    } else s.vehicle.heading_deg.reset();
}

void Decoder::gpsCompass(ADASState& s, const std::array<uint8_t,8>& d) {
    const int raw = static_cast<int>(be_raw(d.data(), d.size(), 26, 4));
    if (raw >= 0 && raw <= 7) s.vehicle.compass_dir = raw;
    else s.vehicle.compass_dir.reset();
}

void Decoder::engineLamps(ADASState& s, const std::array<uint8_t,8>& d) {
    s.telltales.mil = static_cast<int>(be_raw(d.data(), d.size(), 3, 2));
    s.telltales.oil_pressure = be_raw(d.data(), d.size(), 18, 1) != 0;
}

void Decoder::absStatus(ADASState& s, const std::array<uint8_t,8>& d) {
    s.telltales.traction_lamp = static_cast<int>(be_raw(d.data(), d.size(), 42, 2));
    s.telltales.abs_fault = be_raw(d.data(), d.size(), 5, 1) != 0;
    s.telltales.abs_lamp = static_cast<int>(be_raw(d.data(), d.size(), 55, 2));
}

void Decoder::brakeStatus(ADASState& s, const std::array<uint8_t,8>& d) {
    s.telltales.brake_fluid = static_cast<int>(be_raw(d.data(), d.size(), 17, 2));
}

void Decoder::steeringWheel(ADASState& s, const std::array<uint8_t,8>& d) {
    const bool down = be_raw(d.data(), d.size(), 0, 1) != 0;
    const bool up   = be_raw(d.data(), d.size(), 3, 1) != 0;
    const bool ok   = be_raw(d.data(), d.size(), 4, 1) != 0;
    constexpr int count = static_cast<int>(CenterView::Count);
    int view = static_cast<int>(s.center_view);
    if (down && !prev_down_) view = (view + 1) % count;
    if (up && !prev_up_) view = (view + count - 1) % count;
    if (ok && !prev_ok_ && s.ok_press_count != std::numeric_limits<std::uint64_t>::max()) ++s.ok_press_count;
    s.center_view = static_cast<CenterView>(view);
    prev_down_ = down;
    prev_up_ = up;
    prev_ok_ = ok;
}

void Decoder::gapButtons(ADASState& s, const std::array<uint8_t,8>& d) {
    const bool dec = be_raw(d.data(), d.size(), 12, 1) != 0;
    const bool inc = be_raw(d.data(), d.size(), 11, 1) != 0;
    const bool toggle = be_raw(d.data(), d.size(), 32, 1) != 0;
    int gap = (s.follow_gap >= 1 && s.follow_gap <= 4) ? s.follow_gap : 1;
    if (inc && !prev_gap_inc_) { gap = std::min(4, gap + 1); s.follow_gap_display = true; }
    if (dec && !prev_gap_dec_) { gap = std::max(1, gap - 1); s.follow_gap_display = true; }
    if (toggle && !prev_gap_toggle_) { gap = (gap % 4) + 1; s.follow_gap_display = true; }
    if ((inc && !prev_gap_inc_) || (dec && !prev_gap_dec_) || (toggle && !prev_gap_toggle_))
        s.follow_gap = gap;
    prev_gap_inc_ = inc;
    prev_gap_dec_ = dec;
    prev_gap_toggle_ = toggle;
}

void Decoder::bodyInfo(ADASState& s, const std::array<uint8_t,8>& d, double t) {
    s.body.liftgate_ajar       = be_raw(d.data(), d.size(), 0, 1) != 0;
    s.body.rear_left_ajar      = be_raw(d.data(), d.size(), 48, 1) != 0;
    s.body.rear_right_ajar     = be_raw(d.data(), d.size(), 49, 1) != 0;
    s.body.passenger_front_ajar= be_raw(d.data(), d.size(), 60, 1) != 0;
    s.body.hood_ajar           = be_raw(d.data(), d.size(), 59, 1) != 0;
    s.body.driver_front_ajar   = be_raw(d.data(), d.size(), 61, 1) != 0;
    s.body.updated = t;

    // Ignition_Status is carried in the same BodyInfo_3 frame. Ford's
    // generic table defines value 2 as Accessory.  We only use the rising
    // transition into Accessory for the transient IPC message.
    s.warnings.ignition_status = static_cast<int>(be_raw(d.data(), d.size(), 7, 4));
    s.warnings.full_accessory_power = (s.warnings.ignition_status == 2);
    s.warnings.updated = t;
}

void Decoder::engineService(ADASState& s, const std::array<uint8_t,8>& d, double t) {
    // EngineData_7 / EngSrvcRqd_B_Rq : 19|1@0+
    // This is the service/wrench request used for the yellow "See Manual"
    // message.  Do not alias it to the red Powertrain Malfunction warning.
    s.warnings.see_manual = be_raw(d.data(), d.size(), 19, 1) != 0;
    s.warnings.updated = t;
}

void Decoder::lowVoltagePower(ADASState& s, const std::array<uint8_t,8>& d, double t) {
    // Low_Voltage_Power_Data_FD1 / PwSysULoFalt_D_Stat : 12|4@0+
    // 1 is explicitly cluster prove-out.  The states below represent real
    // output/voltage/monitor faults suitable for the Check Charging System
    // popup, while low-SOC/power-save states are left alone.
    const int fault = static_cast<int>(be_raw(d.data(), d.size(), 12, 4));
    s.warnings.low_voltage_fault_state = fault;
    s.warnings.charging_system_fault =
        fault == 2 || fault == 3 || fault == 4 || fault == 5 || fault == 8 || fault == 9;
    s.warnings.updated = t;
}

void Decoder::apimTrack(ADASState& s, const std::array<uint8_t,8>& d, double t) {
    const uint32_t active = be_raw(d.data(), d.size(), 7, 32);
    const uint16_t count = static_cast<uint16_t>(be_raw(d.data(), d.size(), 39, 16));
    s.media.track_number = (active == 0xFFFFFFFFU) ? std::nullopt : std::optional<uint32_t>(active);
    s.media.track_count = (count == 0xFFFFU) ? std::nullopt : std::optional<uint16_t>(count);
    s.media.updated = t;
}

void Decoder::acc(ADASState& s, const std::array<uint8_t,8>& d) {
    // Ford ACCDATA_3 / 0x18A: the IPMA publishes the setting the IPC is
    // supposed to display.  The driver's selection originates at the
    // steering controls, but the cluster does not need the private/raw SCCM
    // command in order to render the OEM following-gap graphic.
    s.follow_mode = be_raw(d.data(), d.size(), 17, 1) != 0;
    s.follow_gap_display = be_raw(d.data(), d.size(), 35, 1) != 0;
    const int gap = static_cast<int>(be_raw(d.data(), d.size(), 34, 3));
    s.follow_gap = (gap >= 1 && gap <= 4) ? gap : 0;
}

} // namespace fordadas
