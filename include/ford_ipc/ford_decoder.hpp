#pragma once

#include "ford_ipc/bitfield.hpp"
#include "ford_ipc/model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fordadas {

enum class DecodeResult : std::uint8_t {
    Accepted = 0,
    UnsupportedId,
    InvalidLength,
    InvalidTimestamp,
    FreshnessTableFull,
    StateAccessFailure
};

class Decoder {
public:
    static constexpr std::uint32_t ID_ACCDATA_3       = 0x18A;
    static constexpr std::uint32_t ID_SIDE_LEFT       = 0x3A6;
    static constexpr std::uint32_t ID_SIDE_RIGHT      = 0x3A7;
    static constexpr std::uint32_t ID_LANE_ASSIST     = 0x3CA;
    static constexpr std::uint32_t ID_TRAFFIC_RECOG   = 0x3CD;
    static constexpr std::uint32_t ID_LATERAL_MOTION  = 0x3D3;
    static constexpr std::uint32_t ID_STEER_ASSIST    = 0x3D7;
    static constexpr std::uint32_t ID_ODOMETER        = 0x166;
    static constexpr std::uint32_t ID_PRNDL           = 0x176;
    static constexpr std::uint32_t ID_AMBIENT_TEMP    = 0x178;
    static constexpr std::uint32_t ID_VEHICLE_SPEED   = 0x202;
    static constexpr std::uint32_t ID_ENGINE_RPM      = 0x204;
    static constexpr std::uint32_t ID_BRAKE_STATUS    = 0x415;
    static constexpr std::uint32_t ID_ABS_STATUS      = 0x416;
    static constexpr std::uint32_t ID_ENGINE_LAMPS    = 0x421;
    static constexpr std::uint32_t ID_DTE             = 0x424;
    static constexpr std::uint32_t ID_GPS_COMPASS     = 0x463;
    static constexpr std::uint32_t ID_GPS_HEADING     = 0x464;
    static constexpr std::uint32_t ID_STEERING_WHEEL  = 0x081;
    static constexpr std::uint32_t ID_STEERING_DATA   = 0x083;
    static constexpr std::uint32_t ID_BODY_INFO       = 0x3B3;
    static constexpr std::uint32_t ID_APIM_TRACK      = 0x3F1;
    static constexpr std::uint32_t ID_ENGINE_SERVICE  = 0x179;
    static constexpr std::uint32_t ID_LOW_VOLTAGE_PWR = 0x42C;

    static constexpr std::size_t SupportedIdCount = 24U;
    static const std::array<std::uint32_t, SupportedIdCount>& supportedIds() noexcept;
    static bool isSupportedId(std::uint32_t id) noexcept;

    explicit Decoder(SharedState& shared) noexcept : shared_(shared) {}
    DecodeResult feed(std::uint32_t can_id, const std::uint8_t* data, std::size_t len, double now);

private:
    SharedState& shared_;
    std::uint64_t rate_frames_ = 0U;
    double rate_window_start_ = 0.0;
    bool prev_up_ = false;
    bool prev_down_ = false;
    bool prev_ok_ = false;
    bool prev_gap_inc_ = false;
    bool prev_gap_dec_ = false;
    bool prev_gap_toggle_ = false;

    static void lane(ADASState&, const std::array<std::uint8_t,8>&, double);
    static void lateralMotion(ADASState&, const std::array<std::uint8_t,8>&, double);
    static void lead(ADASState&, const std::array<std::uint8_t,8>&, double);
    static void traffic(ADASState&, const std::array<std::uint8_t,8>&, double);
    static void side(SideStatus&, const std::array<std::uint8_t,8>&, double);
    static void vehicleSpeed(ADASState&, const std::array<std::uint8_t,8>&);
    static void engineRpm(ADASState&, const std::array<std::uint8_t,8>&);
    static void prndl(ADASState&, const std::array<std::uint8_t,8>&);
    static void dte(ADASState&, const std::array<std::uint8_t,8>&);
    static void ambientTemp(ADASState&, const std::array<std::uint8_t,8>&);
    static void odometer(ADASState&, const std::array<std::uint8_t,8>&);
    static void gpsHeading(ADASState&, const std::array<std::uint8_t,8>&);
    static void gpsCompass(ADASState&, const std::array<std::uint8_t,8>&);
    static void engineLamps(ADASState&, const std::array<std::uint8_t,8>&);
    static void absStatus(ADASState&, const std::array<std::uint8_t,8>&);
    static void brakeStatus(ADASState&, const std::array<std::uint8_t,8>&);
    void steeringWheel(ADASState&, const std::array<std::uint8_t,8>&);
    void gapButtons(ADASState&, const std::array<std::uint8_t,8>&);
    static void bodyInfo(ADASState&, const std::array<std::uint8_t,8>&, double);
    static void engineService(ADASState&, const std::array<std::uint8_t,8>&, double);
    static void lowVoltagePower(ADASState&, const std::array<std::uint8_t,8>&, double);
    static void apimTrack(ADASState&, const std::array<std::uint8_t,8>&, double);
    static void acc(ADASState&, const std::array<std::uint8_t,8>&);
};

} // namespace fordadas
