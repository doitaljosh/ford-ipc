#pragma once

#include "ford_ipc/model.hpp"
#include "ford_ipc/time.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace fordadas {

enum class SafetyFault : std::uint32_t {
    None                 = 0U,
    DecoderInput         = 1U << 0U,
    CanTransport         = 1U << 1U,
    CanThreadHeartbeat   = 1U << 2U,
    CanDataTimeout       = 1U << 3U,
    RenderHeartbeat      = 1U << 4U,
    StatePlausibility    = 1U << 5U,
    ResourceIntegrity    = 1U << 6U,
    FreshnessTableFull   = 1U << 7U,
    SharedStateAccess    = 1U << 8U,
    ClockRegression      = 1U << 9U,
    PopupOverflow        = 1U << 10U,
    WatchdogStart        = 1U << 11U,
    ReceiverStart        = 1U << 12U,
    AudioOutput          = 1U << 13U
};

constexpr std::uint32_t faultMask(SafetyFault f) noexcept {
    return static_cast<std::uint32_t>(f);
}

enum class SafetyMode : std::uint8_t {
    Normal = 0,
    Degraded = 1,
    SafeState = 2
};

struct SafetyHealth {
    std::uint32_t latched_faults = 0U;
    bool render_alive = false;
    bool can_thread_alive = false;
    bool can_data_alive = false;
    bool can_expected = false;
    SafetyMode mode = SafetyMode::Normal;
};

class SafetyMonitor {
public:
    static constexpr MonotonicMs RenderTimeoutMs = 500U;
    static constexpr MonotonicMs CanThreadTimeoutMs = 750U;
    static constexpr MonotonicMs CanDataTimeoutMs = 1500U;

    void setCanExpected(bool expected) noexcept { can_expected_.store(expected, std::memory_order_release); }
    void heartbeatRender(MonotonicMs now) noexcept;
    void heartbeatCanThread(MonotonicMs now) noexcept;
    void heartbeatCanData(MonotonicMs now) noexcept;
    void report(SafetyFault fault) noexcept;
    SafetyHealth evaluate(MonotonicMs now) const noexcept;
    std::uint32_t latchedFaults() const noexcept { return faults_.load(std::memory_order_acquire); }

private:
    std::atomic<std::uint32_t> faults_{0U};
    std::atomic<MonotonicMs> render_heartbeat_{0U};
    std::atomic<MonotonicMs> can_thread_heartbeat_{0U};
    std::atomic<MonotonicMs> can_data_heartbeat_{0U};
    std::atomic<bool> can_expected_{false};

    void heartbeat(std::atomic<MonotonicMs>& target, MonotonicMs now) noexcept;
};

class SafetyWatchdog {
public:
    explicit SafetyWatchdog(SafetyMonitor& monitor) noexcept : monitor_(monitor) {}
    ~SafetyWatchdog();
    SafetyWatchdog(const SafetyWatchdog&) = delete;
    SafetyWatchdog& operator=(const SafetyWatchdog&) = delete;
    bool start() noexcept;
    void stop() noexcept;
private:
    SafetyMonitor& monitor_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> started_{false};
    std::thread thread_;
    void run() noexcept;
};

[[nodiscard]] SafetyMode safetyModeForFaults(std::uint32_t mask) noexcept;
[[nodiscard]] bool validateSnapshotPlausibility(const ADASState& s) noexcept;
void applySafetyDisplayPolicy(ADASState& s, double now_seconds, bool live_can,
                              SafetyMode mode, std::uint32_t faults) noexcept;
std::string safetyFaultText(std::uint32_t mask);

} // namespace fordadas
