#pragma once

#include "ford_ipc/cluster_logic.hpp"
#include "ford_ipc/model.hpp"

#include <SDL2/SDL.h>

#include <filesystem>
#include <memory>
#include <string>

namespace fordadas {

class ClusterUi {
public:
    ClusterUi(SDL_Renderer* renderer,
              const std::filesystem::path& assetPath,
              const std::filesystem::path& fontPath,
              double rasterScale,
              int sceneWidth,
              int sceneHeight);
    ~ClusterUi();

    ClusterUi(const ClusterUi&) = delete;
    ClusterUi& operator=(const ClusterUi&) = delete;

    void renderFrame(const ADASState& state,
                     const TripManager& trips,
                     bool debug,
                     const std::string& distanceUnits,
                     const StartupPostVisual& post,
                     const PageTransitionVisual& page,
                     const VehiclePanelVisual& vehicleVisual,
                     const PopupVisual& popup,
                     int rotation);

    // Allocation-free visual fallback used after a runtime HMI/resource failure.
    // It deliberately avoids fonts/assets and relies only on SDL primitives.
    void renderEmergencyFrame(int rotation) noexcept;

    const std::filesystem::path& assetRoot() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fordadas
