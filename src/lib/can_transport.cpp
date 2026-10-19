#include "ford_ipc/can_transport.hpp"
#include "ford_ipc/time.hpp"

#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

namespace fordadas {
namespace {

struct CanOpenResult {
    int fd = -1;
    std::string error;
};

CanOpenResult openCanReadOnly(const std::string& name) {
    CanOpenResult out{};
    if (name.empty() || name.size() >= IFNAMSIZ) {
        out.error = "invalid CAN interface name";
        return out;
    }
    const int fd = ::socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW);
    if (fd < 0) {
        out.error = "socket(PF_CAN): " + std::string(std::strerror(errno));
        return out;
    }

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1U);
    ifr.ifr_name[IFNAMSIZ - 1U] = '\0';
    if (::ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        out.error = "unknown CAN interface " + name;
        ::close(fd);
        return out;
    }

    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        out.error = "bind(" + name + "): " + std::string(std::strerror(errno));
        ::close(fd);
        return out;
    }

    std::array<can_filter, Decoder::SupportedIdCount> filters{};
    const auto& ids = Decoder::supportedIds();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        filters[i].can_id = ids[i];
        filters[i].can_mask = CAN_SFF_MASK;
    }
    if (::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, filters.data(),
                     static_cast<socklen_t>(sizeof(filters))) < 0) {
        out.error = "CAN_RAW_FILTER: " + std::string(std::strerror(errno));
        ::close(fd);
        return out;
    }

    const can_err_mask_t errorMask = CAN_ERR_BUSOFF | CAN_ERR_CRTL | CAN_ERR_RESTARTED;
    if (::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &errorMask, sizeof(errorMask)) < 0) {
        out.error = "CAN_RAW_ERR_FILTER: " + std::string(std::strerror(errno));
        ::close(fd);
        return out;
    }

    const int recvOwn = 0;
    (void)::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recvOwn, sizeof(recvOwn));

    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        out.error = "fcntl(O_NONBLOCK): " + std::string(std::strerror(errno));
        ::close(fd);
        return out;
    }

    out.fd = fd;
    return out;
}

struct ReplayFrame {
    double ts = 0.0;
    std::uint32_t id = 0U;
    std::array<std::uint8_t,8> data{};
    std::size_t len = 0U;
};

bool loadReplay(const std::string& path, std::vector<ReplayFrame>& out, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open replay: " + path;
        return false;
    }
    std::string line;
    double synthetic = 0.0;
    while (std::getline(in, line)) {
        ReplayFrame f{};
        std::string rest = line;
        try {
            if (!rest.empty() && rest.front() == '(') {
                const auto e = rest.find(')');
                if (e == std::string::npos) continue;
                f.ts = std::stod(rest.substr(1U, e - 1U));
                rest = rest.substr(e + 1U);
            } else {
                f.ts = synthetic;
                synthetic += 0.01;
            }
            std::istringstream iss(rest);
            std::string iface;
            std::string frame;
            if (!(iss >> iface >> frame)) continue;
            const auto hash = frame.find('#');
            if (hash == std::string::npos) continue;
            f.id = static_cast<std::uint32_t>(std::stoul(frame.substr(0U, hash), nullptr, 16));
            if (!Decoder::isSupportedId(f.id)) continue;
            const std::string hex = frame.substr(hash + 1U);
            if (hex.size() != 16U) continue; // this cluster's consumed frames are 8-byte classic CAN
            f.len = 8U;
            for (std::size_t i = 0; i < f.len; ++i) {
                f.data[i] = static_cast<std::uint8_t>(std::stoul(hex.substr(i * 2U, 2U), nullptr, 16));
            }
            out.push_back(f);
        } catch (...) {
            continue; // malformed replay line is ignored, never allowed to affect live state
        }
    }
    if (out.empty()) {
        error = "no valid 8-byte supported candump frames parsed";
        return false;
    }
    return true;
}

} // namespace

SocketCANReceiver::SocketCANReceiver(SharedState& state, Decoder& decoder, SafetyMonitor& safety, std::string iface)
    : state_(state), decoder_(decoder), safety_(safety), iface_(std::move(iface)) {}

SocketCANReceiver::~SocketCANReceiver() { stop(); }

bool SocketCANReceiver::start() noexcept {
    stop_.store(false, std::memory_order_release);
    safety_.setCanExpected(true);
    const auto start_ms = monotonicMilliseconds();
    safety_.heartbeatCanThread(start_ms);
    // Grace period starts here; this heartbeat is replaced only by accepted CAN data.
    safety_.heartbeatCanData(start_ms);
    try {
        thread_ = std::thread([this] { run(); });
        return true;
    } catch (...) {
        safety_.report(SafetyFault::ReceiverStart);
        safety_.setCanExpected(false);
        return false;
    }
}

void SocketCANReceiver::stop() noexcept {
    stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
}

void SocketCANReceiver::run() noexcept {
    const auto opened = openCanReadOnly(iface_);
    if (opened.fd < 0) {
        safety_.report(SafetyFault::CanTransport);
        if (!state_.tryMutate([&](ADASState& s) { s.mode_text = "CAN ERROR"; s.last_error = opened.error; }))
            safety_.report(SafetyFault::SharedStateAccess);
        safety_.setCanExpected(false);
        return;
    }
    const int fd = opened.fd;
    if (!state_.tryMutate([&](ADASState& s) {
            s.mode_text.assignJoined("LIVE ", iface_);
            s.last_error.clear();
        })) safety_.report(SafetyFault::SharedStateAccess);
    pollfd p{fd, static_cast<short>(POLLIN | POLLERR | POLLHUP | POLLNVAL), 0};

    while (!stop_.load(std::memory_order_acquire)) {
        safety_.heartbeatCanThread(monotonicMilliseconds());
        const int rc = ::poll(&p, 1, 100);
        if (rc < 0) {
            if (errno == EINTR) continue;
            safety_.report(SafetyFault::CanTransport);
            break;
        }
        if (rc == 0) continue;
        if ((p.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            safety_.report(SafetyFault::CanTransport);
            break;
        }
        if ((p.revents & POLLIN) == 0) continue;

        for (;;) {
            can_frame fr{};
            const ssize_t n = ::read(fd, &fr, sizeof(fr));
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                safety_.report(SafetyFault::CanTransport);
                break;
            }
            if (n != static_cast<ssize_t>(sizeof(fr))) {
                safety_.report(SafetyFault::DecoderInput);
                break;
            }
            if ((fr.can_id & CAN_ERR_FLAG) != 0U) {
                safety_.report(SafetyFault::CanTransport);
                continue;
            }
            if ((fr.can_id & (CAN_RTR_FLAG | CAN_EFF_FLAG)) != 0U) continue;
            if (fr.can_dlc != 8U) {
                safety_.report(SafetyFault::DecoderInput);
                continue;
            }
            const auto result = decoder_.feed(fr.can_id & CAN_SFF_MASK, fr.data, 8U, monotonicSeconds());
            if (result == DecodeResult::Accepted) {
                safety_.heartbeatCanData(monotonicMilliseconds());
            } else if (result == DecodeResult::FreshnessTableFull) {
                safety_.heartbeatCanData(monotonicMilliseconds());
                safety_.report(SafetyFault::FreshnessTableFull);
            } else if (result == DecodeResult::StateAccessFailure) {
                safety_.report(SafetyFault::SharedStateAccess);
            } else if (result != DecodeResult::UnsupportedId) {
                safety_.report(SafetyFault::DecoderInput);
            }
        }
    }
    ::close(fd);
    safety_.setCanExpected(false);
}

ReplayReceiver::ReplayReceiver(SharedState& state, Decoder& decoder, SafetyMonitor& safety,
                               std::string path, double speed, bool loop)
    : state_(state), decoder_(decoder), safety_(safety), path_(std::move(path)),
      speed_(std::max(0.01, speed)), loop_(loop) {}

ReplayReceiver::~ReplayReceiver() { stop(); }

bool ReplayReceiver::start() noexcept {
    stop_.store(false, std::memory_order_release);
    safety_.setCanExpected(false);
    try {
        thread_ = std::thread([this] { run(); });
        return true;
    } catch (...) {
        safety_.report(SafetyFault::ReceiverStart);
        return false;
    }
}

void ReplayReceiver::stop() noexcept {
    stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
}

void ReplayReceiver::run() noexcept {
    std::vector<ReplayFrame> frames;
    std::string error;
    if (!loadReplay(path_, frames, error)) {
        safety_.report(SafetyFault::DecoderInput);
        if (!state_.tryMutate([&](ADASState& s) { s.mode_text = "REPLAY ERROR"; s.last_error = error; }))
            safety_.report(SafetyFault::SharedStateAccess);
        return;
    }
    if (!state_.tryMutate([&](ADASState& s) {
        s.mode_text.assignJoined("REPLAY ", std::filesystem::path(path_).filename().string());
        s.last_error.clear();
    })) safety_.report(SafetyFault::SharedStateAccess);

    do {
        const double base = frames.front().ts;
        const double start = monotonicSeconds();
        for (const auto& f : frames) {
            if (stop_.load(std::memory_order_acquire)) return;
            const double target = start + (f.ts - base) / speed_;
            while (!stop_.load(std::memory_order_acquire) && monotonicSeconds() < target)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (stop_.load(std::memory_order_acquire)) return;
            const auto result = decoder_.feed(f.id, f.data.data(), f.len, monotonicSeconds());
            if (result == DecodeResult::FreshnessTableFull) safety_.report(SafetyFault::FreshnessTableFull);
            else if (result == DecodeResult::StateAccessFailure) safety_.report(SafetyFault::SharedStateAccess);
            else if (result != DecodeResult::Accepted) safety_.report(SafetyFault::DecoderInput);
        }
    } while (loop_ && !stop_.load(std::memory_order_acquire));
}

} // namespace fordadas
