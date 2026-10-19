#include "ford_ipc/app_config.hpp"
#include "ford_ipc/app_services.hpp"
#include "ford_ipc/can_transport.hpp"
#include "ford_ipc/chime_audio.hpp"
#include "ford_ipc/cluster_logic.hpp"
#include "ford_ipc/ford_decoder.hpp"
#include "ford_ipc/layout.hpp"
#include "ford_ipc/safety.hpp"
#include "ford_ipc/time.hpp"
#include "ford_ipc/ui.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

using namespace fordadas;

namespace {

struct SdlRuntime {
    SdlRuntime() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) throw std::runtime_error(SDL_GetError());
        if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) throw std::runtime_error(IMG_GetError());
        if (TTF_Init() != 0) throw std::runtime_error(TTF_GetError());
    }
    ~SdlRuntime() {
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    }
};

struct WindowDeleter { void operator()(SDL_Window* p) const noexcept { if (p) SDL_DestroyWindow(p); } };
struct RendererDeleter { void operator()(SDL_Renderer* p) const noexcept { if (p) SDL_DestroyRenderer(p); } };
using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;

bool keyboardSccmEvent(SharedState& shared, SDL_Keycode key) noexcept {
    return shared.tryMutate([key](ADASState& s) {
        if (key == SDLK_DOWN) s.center_view = nextCenterView(s.center_view);
        else if (key == SDLK_UP) s.center_view = previousCenterView(s.center_view);
        else if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && s.ok_press_count != std::numeric_limits<std::uint64_t>::max()) ++s.ok_press_count;
    });
}

bool keyboardBodyTestEvent(SharedState& shared, SDL_Keycode key, double stamp) noexcept {
    return shared.tryMutate([key, stamp](ADASState& s) {
        if (key == SDLK_F8) s.body.driver_front_ajar = !s.body.driver_front_ajar;
        else if (key == SDLK_F9) s.body.passenger_front_ajar = !s.body.passenger_front_ajar;
        else if (key == SDLK_F10) s.body.rear_left_ajar = !s.body.rear_left_ajar;
        else if (key == SDLK_F11) s.body.rear_right_ajar = !s.body.rear_right_ajar;
        else if (key == SDLK_h) s.body.hood_ajar = !s.body.hood_ajar;
        else if (key == SDLK_l) s.body.liftgate_ajar = !s.body.liftgate_ajar;
        s.body.updated = stamp;
    });
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Config cfg = parseArgs(argc, argv);
        SdlRuntime sdl;

        SDL_SetHint(SDL_HINT_RENDER_DRIVER, cfg.renderer.c_str());
        SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

        Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
        if (!cfg.windowed) windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        WindowPtr window(SDL_CreateWindow("Ford IPC Driver Assist - GE 1280x800",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            cfg.window_w, cfg.window_h, windowFlags));
        if (!window) throw std::runtime_error("SDL_CreateWindow: " + std::string(SDL_GetError()));

        constexpr Uint32 PreferredFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE;
        RendererPtr renderer(SDL_CreateRenderer(window.get(), -1, PreferredFlags));
        if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE));
        if (!renderer) throw std::runtime_error("hardware renderer unavailable: " + std::string(SDL_GetError()));

        SDL_RendererInfo rendererInfo{};
        if (SDL_GetRendererInfo(renderer.get(), &rendererInfo) != 0)
            throw std::runtime_error("SDL_GetRendererInfo: " + std::string(SDL_GetError()));
        if ((rendererInfo.flags & SDL_RENDERER_ACCELERATED) == 0U)
            throw std::runtime_error("refusing software renderer: GPU acceleration is required");

        std::cout << "SDL video driver: " << (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "?") << '\n';
        std::cout << "SDL renderer: " << (rendererInfo.name ? rendererInfo.name : "?") << " [GPU]"
                  << ((rendererInfo.flags & SDL_RENDERER_PRESENTVSYNC) ? " [vsync]" : "") << '\n';

        int outputW = 0, outputH = 0;
        if (SDL_GetRendererOutputSize(renderer.get(), &outputW, &outputH) != 0)
            throw std::runtime_error("SDL_GetRendererOutputSize: " + std::string(SDL_GetError()));
        int rotation = cfg.rotation < 0 ? ((outputH > outputW) ? 90 : 0) : normalizedRotation(cfg.rotation);
        const double rasterScale = chooseRasterScale(cfg, outputW, outputH, rotation);
        const int sceneW = std::max(LogicalWidth, static_cast<int>(std::lround(LogicalWidth * rasterScale)));
        const int sceneH = std::max(LogicalHeight, static_cast<int>(std::lround(LogicalHeight * rasterScale)));
        std::cout << "Renderer output: " << outputW << 'x' << outputH
                  << ", IPC internal render: " << sceneW << 'x' << sceneH
                  << " (" << std::fixed << std::setprecision(2) << rasterScale << "x)\n";

        const auto assetPath = resolveAssets(cfg, argv[0]);
        const auto fontPath = resolveFonts(cfg, assetPath);
        std::cout << "IPC assets: " << assetPath << '\n';
        std::cout << "Ford Antenna fonts: " << fontPath << '\n';
        ClusterUi ui(renderer.get(), assetPath, fontPath, rasterScale, sceneW, sceneH);

        SharedState shared;
        if (!cfg.demo && cfg.can_iface.empty() && cfg.replay.empty()) initializeStandbyState(shared);

        SafetyMonitor safety;
        FordChimeAudio chime(cfg.chime_volume);
        if (cfg.chimes_enabled && !chime.open()) {
            safety.report(SafetyFault::AudioOutput);
            std::cerr << "warning: local chime audio unavailable: " << SDL_GetError() << '\n';
        }
        safety.heartbeatRender(monotonicMilliseconds());
        SafetyWatchdog watchdog(safety);
        (void)watchdog.start();

        Decoder decoder(shared);
        TripManager trips;
        VehicleAnimator vehicleAnimator;
        PopupManager popupManager;
        MediaFileWatcher mediaWatcher(cfg.media_file);
        std::unique_ptr<SocketCANReceiver> live;
        std::unique_ptr<ReplayReceiver> replay;
        if (!cfg.can_iface.empty()) {
            live = std::make_unique<SocketCANReceiver>(shared, decoder, safety, cfg.can_iface);
            (void)live->start();
        } else if (!cfg.replay.empty()) {
            replay = std::make_unique<ReplayReceiver>(shared, decoder, safety, cfg.replay, cfg.replay_speed, cfg.replay_loop);
            (void)replay->start();
        } else {
            safety.setCanExpected(false);
        }

        bool running = true;
        bool debug = cfg.debug;
        PageTransition pageTransition(CenterView::ADAS);
        const double appStart = monotonicSeconds();
        constexpr double PostDuration = 2.55;
        const double demoStart = appStart + PostDuration;
        const double framePeriod = 1.0 / static_cast<double>(std::max(10, cfg.fps));
        double nextFrame = monotonicSeconds();
        std::uint32_t lastFaultMask = 0U;
        std::uint64_t lastChimedPopupSequence = 0U;
        double nextAudioHealthCheck = appStart;
        bool hmiFailed = false;

        while (running) {
            SDL_Event event{};
            while (SDL_PollEvent(&event) != 0) {
                if (event.type == SDL_QUIT) running = false;
                else if (event.type == SDL_KEYDOWN) {
                    const SDL_Keycode key = event.key.keysym.sym;
                    const bool sccmKey = key == SDLK_UP || key == SDLK_DOWN || key == SDLK_RETURN || key == SDLK_KP_ENTER;
                    if (sccmKey && event.key.repeat != 0) continue; // SCCM is edge-triggered
                    if (key == SDLK_ESCAPE) running = false;
                    else if (key == SDLK_d) debug = !debug;
                    else if (key == SDLK_r) rotation = (rotation + 90) % 360;
                    else if (sccmKey) { if (!keyboardSccmEvent(shared, key)) safety.report(SafetyFault::SharedStateAccess); }
                    else if (key == SDLK_F1) popupManager.testPopup(PopupKind::AccessoryPower);
                    else if (key == SDLK_F2) popupManager.testPopup(PopupKind::WasherFluid);
                    else if (key == SDLK_F3) popupManager.testPopup(PopupKind::SeeManual);
                    else if (key == SDLK_F4) popupManager.testPopup(PopupKind::Powertrain);
                    else if (key == SDLK_F5) popupManager.testPopup(PopupKind::EngineOverTemperature);
                    else if (key == SDLK_F6) popupManager.testPopup(PopupKind::EngineOilPressureLow);
                    else if (key == SDLK_F7) popupManager.testPopup(PopupKind::ChargingSystem);
                    else if ((key >= SDLK_F8 && key <= SDLK_F11) || key == SDLK_h || key == SDLK_l) {
                        if (!keyboardBodyTestEvent(shared, key, monotonicSeconds())) safety.report(SafetyFault::SharedStateAccess);
                    }
                    else if (key == SDLK_f) {
                        const Uint32 flags = SDL_GetWindowFlags(window.get());
                        const Uint32 mode = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0U : SDL_WINDOW_FULLSCREEN_DESKTOP;
                        if (SDL_SetWindowFullscreen(window.get(), mode) != 0)
                            safety.report(SafetyFault::ResourceIntegrity);
                    }
                }
            }

            const double now = monotonicSeconds();
            if (cfg.demo && now >= demoStart) updateDemo(shared, now - demoStart);
            mediaWatcher.poll(shared, now);
            const StartupPostVisual post = startupPostVisual(now - appStart);

            if (now >= nextFrame) {
                ADASState snap{};
                if (!shared.trySnapshot(snap)) safety.report(SafetyFault::SharedStateAccess);
                if (!validateSnapshotPlausibility(snap)) safety.report(SafetyFault::StatePlausibility);
                if (snap.freshness.overflowed()) safety.report(SafetyFault::FreshnessTableFull);

                const auto nowMs = monotonicMilliseconds();
                const auto health = safety.evaluate(nowMs);
                if (!health.render_alive) safety.report(SafetyFault::RenderHeartbeat);
                if (!health.can_thread_alive) safety.report(SafetyFault::CanThreadHeartbeat);
                if (!health.can_data_alive) safety.report(SafetyFault::CanDataTimeout);

                std::uint32_t faults = safety.latchedFaults();
                const SafetyMode mode = safetyModeForFaults(faults);
                applySafetyDisplayPolicy(snap, now, !cfg.can_iface.empty(), mode, faults);

                const std::size_t droppedBefore = popupManager.droppedEvents();
                const bool okConsumed = popupManager.update(snap, now);
                if (popupManager.droppedEvents() != droppedBefore) {
                    safety.report(SafetyFault::PopupOverflow);
                    faults = safety.latchedFaults();
                    snap.integrity_faults = faults;
                    if (snap.integrity_state == IntegrityState::Nominal) snap.integrity_state = IntegrityState::Degraded;
                }
                trips.update(snap, now, !okConsumed);
                const auto vehicleVisual = vehicleAnimator.update(snap.body, now);
                const auto popup = popupManager.visual(now);

                if (cfg.chimes_enabled && chime.available()) {
                    const bool passengerDoorOpen = snap.body.driver_front_ajar || snap.body.passenger_front_ajar ||
                                                   snap.body.rear_left_ajar || snap.body.rear_right_ajar;
                    chime.setDoorOpen(passengerDoorOpen);
                    if (popup.active && popup.sequence != 0U && popup.sequence != lastChimedPopupSequence) {
                        lastChimedPopupSequence = popup.sequence;
                        if (popup.event.kind == PopupKind::DoorAjar) chime.notifyDoorOpened();
                        else chime.triggerWarning();
                    }
                    const std::uint32_t newFaults = faults & ~lastFaultMask;
                    if (newFaults != 0U) chime.triggerWarning();
                    if (now >= nextAudioHealthCheck) {
                        if (!chime.healthy()) safety.report(SafetyFault::AudioOutput);
                        nextAudioHealthCheck = now + 1.0;
                    }
                }

                pageTransition.request(snap.center_view, now);
                const auto page = pageTransition.visual(now);

                if (debug && faults != 0U) {
                    const std::string faultText = "SAFETY: " + safetyFaultText(faults);
                    snap.last_error = faultText;
                }
                if (faults != lastFaultMask) {
                    std::cerr << "safety status: " << safetyFaultText(faults) << '\n';
                    lastFaultMask = faults;
                }

                if (!hmiFailed) {
                    try {
                        ui.renderFrame(snap, trips, debug, cfg.distance_units, post, page, vehicleVisual, popup, rotation);
                    } catch (const std::exception& renderError) {
                        safety.report(SafetyFault::ResourceIntegrity);
                        hmiFailed = true;
                        std::cerr << "HMI runtime fault: " << renderError.what() << '\n';
                        ui.renderEmergencyFrame(rotation);
                    } catch (...) {
                        safety.report(SafetyFault::ResourceIntegrity);
                        hmiFailed = true;
                        std::cerr << "HMI runtime fault: unknown exception\n";
                        ui.renderEmergencyFrame(rotation);
                    }
                } else {
                    ui.renderEmergencyFrame(rotation);
                }
                safety.heartbeatRender(monotonicMilliseconds());

                nextFrame += framePeriod;
                if (nextFrame < now - framePeriod) nextFrame = now + framePeriod;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        if (live) live->stop();
        if (replay) replay->stop();
        watchdog.stop();
        return safety.latchedFaults() == 0U ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "ford_adas_ge_ipc: " << e.what() << '\n';
        return 1;
    }
}
