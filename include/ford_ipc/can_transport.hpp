#pragma once

#include "ford_ipc/ford_decoder.hpp"
#include "ford_ipc/safety.hpp"

#include <atomic>
#include <string>
#include <thread>

namespace fordadas {

class SocketCANReceiver {
public:
    SocketCANReceiver(SharedState& state, Decoder& decoder, SafetyMonitor& safety, std::string iface);
    ~SocketCANReceiver();

    SocketCANReceiver(const SocketCANReceiver&) = delete;
    SocketCANReceiver& operator=(const SocketCANReceiver&) = delete;

    bool start() noexcept;
    void stop() noexcept;

private:
    SharedState& state_;
    Decoder& decoder_;
    SafetyMonitor& safety_;
    std::string iface_;
    std::atomic<bool> stop_{false};
    std::thread thread_;

    void run() noexcept;
};

class ReplayReceiver {
public:
    ReplayReceiver(SharedState& state, Decoder& decoder, SafetyMonitor& safety,
                   std::string path, double speed, bool loop);
    ~ReplayReceiver();

    ReplayReceiver(const ReplayReceiver&) = delete;
    ReplayReceiver& operator=(const ReplayReceiver&) = delete;

    bool start() noexcept;
    void stop() noexcept;

private:
    SharedState& state_;
    Decoder& decoder_;
    SafetyMonitor& safety_;
    std::string path_;
    double speed_;
    bool loop_;
    std::atomic<bool> stop_{false};
    std::thread thread_;

    void run() noexcept;
};

} // namespace fordadas
