#pragma once

#include <SDL2/SDL.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace fordadas {

// Local fallback implementation of the Ford IPC/ACM warning-chime role.
//
// The factory 2020 Escape IPC firmware contains SBUS warning-chime commands and
// chime-type enums, but not redistributable WAV/OGG assets. On the vehicle the
// IPC arbitrates the request and normally asks the ACM/DSP to render the chime.
// This class deliberately keeps that audio path isolated from the safety/state
// path: audio failure cannot prevent a visual warning from being displayed.
class FordChimeAudio {
public:
    explicit FordChimeAudio(int volume_percent = 72) noexcept;
    ~FordChimeAudio();

    FordChimeAudio(const FordChimeAudio&) = delete;
    FordChimeAudio& operator=(const FordChimeAudio&) = delete;

    // Open the default SDL output. Failure is non-fatal; caller may report an
    // AUDIO_OUTPUT degraded fault while continuing visual HMI operation.
    bool open() noexcept;
    void close() noexcept;

    [[nodiscard]] bool available() const noexcept { return device_ != 0U; }
    [[nodiscard]] bool healthy() const noexcept;

    // New yellow/red warning or application/safety error. The requested
    // cadence is a descending two-tone pair repeated three times.
    void triggerWarning() noexcept;

    // Door-open state is level-driven so the slow door reminder can continue
    // while a passenger door remains open. notifyDoorOpened() restarts the
    // cadence immediately for each new opening edge.
    void setDoorOpen(bool open) noexcept;
    void notifyDoorOpened() noexcept;

    void setVolume(int volume_percent) noexcept;

    // Exposed for documentation/tests and for the optional preview generator.
    static constexpr int NominalSampleRate = 48000;
    // Keep the device running on digital zero before allowing the first tone.
    // This prevents DAC/mixer wake-up transients from coinciding with a warning.
    static constexpr double StartupWarmupSeconds = 0.200;

    // Frequencies measured directly from the user's 2020 Escape recordings.
    // They form an exact 4:5:6 just-intonation major triad:
    //   440 Hz : 550 Hz : 660 Hz.
    // Door cadence:    440 -> 660 -> 550 Hz.
    // Warning cadence: 440 -> 550 Hz.
    static constexpr double BaseToneHz = 440.0;
    static constexpr double WarningSecondToneHz = 550.0;
    static constexpr double DoorMiddleToneHz = 660.0;

    // Measured note-start timing from the supplied recordings.
    static constexpr double WarningNoteGapSeconds = 0.149;
    static constexpr double WarningPairPeriodSeconds = 0.357;
    static constexpr double DoorNoteGapSeconds = 0.153;
    static constexpr double DoorPeriodSeconds = 1.608;

    // Click-free reconstruction envelope. The audible pitch/cadence is taken
    // from the car recording; each note gets a raised-cosine attack and release
    // so no active sine wave is truncated at a non-zero sample.
    static constexpr double ToneAttackSeconds = 0.030;
    static constexpr double ToneReleaseSeconds = 0.070;
    static constexpr double WarningFirstToneSeconds = 0.500;
    static constexpr double WarningSecondToneSeconds = 0.460;
    static constexpr double DoorFirstToneSeconds = 0.620;
    static constexpr double DoorOtherToneSeconds = 0.580;
    static constexpr double WarningDecayTauSeconds = 0.170;
    static constexpr double DoorDecayTauSeconds = 0.190;
    static constexpr double DoorSequenceSeconds = 0.900;
    static constexpr double WarningSequenceSeconds = 1.350;

private:
    SDL_AudioDeviceID device_ = 0U;
    SDL_AudioSpec obtained_{};
    std::atomic<std::uint32_t> warning_sequence_{0U};
    std::atomic<std::uint32_t> door_sequence_{0U};
    std::atomic<bool> door_open_{false};
    std::atomic<float> volume_{0.72F};

    // Callback-thread-owned state: no lock and no allocation in renderAudio().
    std::uint32_t seen_warning_sequence_ = 0U;
    std::uint32_t seen_door_sequence_ = 0U;
    std::uint64_t sample_clock_ = 0U;
    std::uint64_t warning_start_sample_ = 0U;
    std::uint64_t door_tone_start_sample_ = 0U;
    std::uint64_t next_door_sample_ = 0U;
    std::uint64_t audio_ready_sample_ = 0U;
    bool warning_active_ = false;
    bool door_tone_active_ = false;

    static void sdlAudioCallback(void* userdata, Uint8* stream, int len) noexcept;
    void renderAudio(float* stream, std::size_t frame_count, int channels) noexcept;
    [[nodiscard]] float renderWarningSample(std::uint64_t relative_sample) const noexcept;
    [[nodiscard]] float renderDoorSample(std::uint64_t relative_sample) const noexcept;
    [[nodiscard]] float smoothTone(double frequency_hz, double t, double duration,
                                   double attack_seconds, double decay_tau_seconds,
                                   double release_seconds) const noexcept;
};

} // namespace fordadas
