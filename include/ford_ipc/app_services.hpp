#pragma once

#include "ford_ipc/model.hpp"

#include <string>

namespace fordadas {

class MediaFileWatcher {
public:
    explicit MediaFileWatcher(std::string path);
    void poll(SharedState& shared, double now);
private:
    std::string path_;
    double next_poll_ = 0.0;
};

void updateDemo(SharedState& shared, double t);
void initializeStandbyState(SharedState& shared);

} // namespace fordadas
