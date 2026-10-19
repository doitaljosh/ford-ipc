#include "ford_ipc/chime_audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace fordadas {
namespace {
constexpr double Pi = 3.141592653589793238462643383279502884;

std::uint64_t secondsToSamples(double seconds, int sample_rate) noexcept {
    if (!(seconds > 0.0) || sample_rate <= 0) return 0U;
    const double value = seconds * static_cast<double>(sample_rate);
    if (value >= static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(std::llround(value));
}

float clampAudio(double v) noexcept {
    return static_cast<float>(std::clamp(v, -1.0, 1.0));
}
} // namespace

FordChimeAudio::FordChimeAudio(int volume_percent) noexcept {
    setVolume(volume_percent);
}

FordChimeAudio::~FordChimeAudio() { close(); }

bool FordChimeAudio::open() noexcept {
    if (device_ != 0U) return true;
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0U) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    }

    SDL_AudioSpec wanted{};
    wanted.freq = NominalSampleRate;
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 2U;
    wanted.samples = 512U;
    wanted.callback = &FordChimeAudio::sdlAudioCallback;
    wanted.userdata = this;

    obtained_ = SDL_AudioSpec{};
    constexpr int AllowedChanges = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained_, AllowedChanges);
    if (device_ == 0U) return false;
    if (obtained_.format != AUDIO_F32SYS || obtained_.freq <= 0 || obtained_.channels == 0U) {
        close();
        return false;
    }

    sample_clock_ = 0U;
    warning_active_ = false;
    door_tone_active_ = false;
    seen_warning_sequence_ = warning_sequence_.load(std::memory_order_acquire);
    seen_door_sequence_ = door_sequence_.load(std::memory_order_acquire);
    next_door_sample_ = 0U;
    audio_ready_sample_ = secondsToSamples(StartupWarmupSeconds, obtained_.freq);
    // Start the callback immediately and leave it running continuously. During
    // warm-up it outputs digital zero, so the DAC/mixer settles before any tone.
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void FordChimeAudio::close() noexcept {
    if (device_ == 0U) return;
    SDL_PauseAudioDevice(device_, 1);
    SDL_CloseAudioDevice(device_);
    device_ = 0U;
    obtained_ = SDL_AudioSpec{};
}

bool FordChimeAudio::healthy() const noexcept {
    if (device_ == 0U) return false;
    return SDL_GetAudioDeviceStatus(device_) == SDL_AUDIO_PLAYING;
}

void FordChimeAudio::triggerWarning() noexcept {
    warning_sequence_.fetch_add(1U, std::memory_order_release);
}

void FordChimeAudio::setDoorOpen(bool open) noexcept {
    door_open_.store(open, std::memory_order_release);
}

void FordChimeAudio::notifyDoorOpened() noexcept {
    door_sequence_.fetch_add(1U, std::memory_order_release);
}

void FordChimeAudio::setVolume(int volume_percent) noexcept {
    const int bounded = std::clamp(volume_percent, 0, 100);
    volume_.store(static_cast<float>(bounded) / 100.0F, std::memory_order_release);
}

void FordChimeAudio::sdlAudioCallback(void* userdata, Uint8* stream, int len) noexcept {
    if (stream == nullptr || len <= 0) return;
    std::memset(stream, 0, static_cast<std::size_t>(len));
    if (userdata == nullptr) return;

    auto* self = static_cast<FordChimeAudio*>(userdata);
    const int channels = static_cast<int>(self->obtained_.channels);
    if (channels <= 0) return;
    const std::size_t sample_count = static_cast<std::size_t>(len) / sizeof(float);
    const std::size_t frame_count = sample_count / static_cast<std::size_t>(channels);
    self->renderAudio(reinterpret_cast<float*>(stream), frame_count, channels);
}

float FordChimeAudio::smoothTone(double frequency_hz, double t, double duration,
                                  double attack_seconds, double decay_tau_seconds,
                                  double release_seconds) const noexcept {
    if (t < 0.0 || t >= duration || duration <= 0.0 || frequency_hz <= 0.0) return 0.0F;

    // Click-free note envelope reconstructed from the user's in-car recordings.
    // The previous preview used an exponential decay but truncated the still-live
    // oscillator at the nominal note duration. That creates an instantaneous
    // sample discontinuity (an impulse), which is heard as a pop. Here both ends
    // of every note are guaranteed to approach exactly zero with a cosine taper.
    double attack = 1.0;
    if (attack_seconds > 0.0 && t < attack_seconds) {
        const double x = std::clamp(t / attack_seconds, 0.0, 1.0);
        attack = 0.5 - 0.5 * std::cos(Pi * x);
    }

    const double decay_origin = std::max(0.0, t - std::max(0.0, attack_seconds));
    const double decay = decay_tau_seconds > 0.0
        ? std::exp(-decay_origin / decay_tau_seconds)
        : 1.0;

    double release = 1.0;
    if (release_seconds > 0.0 && t > duration - release_seconds) {
        const double x = std::clamp((t - (duration - release_seconds)) / release_seconds, 0.0, 1.0);
        release = 0.5 + 0.5 * std::cos(Pi * x);
    }

    const double phase = 2.0 * Pi * frequency_hz * t;
    return clampAudio(attack * decay * release * std::sin(phase));
}

float FordChimeAudio::renderWarningSample(std::uint64_t relative_sample) const noexcept {
    const int rate = obtained_.freq;
    if (rate <= 0) return 0.0F;
    const double t = static_cast<double>(relative_sample) / static_cast<double>(rate);

    // Recording-matched warning gesture: 440 -> 550 Hz. The factory recording
    // repeats this pair about every 357 ms. This build keeps the user's original
    // request of three pairs per warning event.
    double mix = 0.0;
    for (int pair = 0; pair < 3; ++pair) {
        const double pair_start = static_cast<double>(pair) * WarningPairPeriodSeconds;
        const double first_local = t - pair_start;
        const double second_local = t - (pair_start + WarningNoteGapSeconds);
        if (first_local >= 0.0 && first_local < WarningFirstToneSeconds) {
            mix += static_cast<double>(smoothTone(
                BaseToneHz, first_local, WarningFirstToneSeconds,
                ToneAttackSeconds, WarningDecayTauSeconds, ToneReleaseSeconds));
        }
        if (second_local >= 0.0 && second_local < WarningSecondToneSeconds) {
            // The upper note is quieter in the supplied warning recording.
            mix += 0.17 * static_cast<double>(smoothTone(
                WarningSecondToneHz, second_local, WarningSecondToneSeconds,
                ToneAttackSeconds, WarningDecayTauSeconds, ToneReleaseSeconds));
        }
    }
    return clampAudio(mix * 0.82);
}

float FordChimeAudio::renderDoorSample(std::uint64_t relative_sample) const noexcept {
    const int rate = obtained_.freq;
    if (rate <= 0) return 0.0F;
    const double t = static_cast<double>(relative_sample) / static_cast<double>(rate);

    // Recording-matched door group: 440 -> 660 -> 550 Hz, approximately 153 ms
    // between note starts. Long decaying tails intentionally overlap, as they do
    // in the car, but every tail ends with a smooth zero-crossing taper.
    double mix = 0.0;
    const double first_local = t;
    const double middle_local = t - DoorNoteGapSeconds;
    const double last_local = t - (2.0 * DoorNoteGapSeconds);

    if (first_local >= 0.0 && first_local < DoorFirstToneSeconds) {
        mix += static_cast<double>(smoothTone(
            BaseToneHz, first_local, DoorFirstToneSeconds,
            0.035, 0.200, 0.080));
    }
    if (middle_local >= 0.0 && middle_local < DoorOtherToneSeconds) {
        mix += 0.37 * static_cast<double>(smoothTone(
            DoorMiddleToneHz, middle_local, DoorOtherToneSeconds,
            0.035, DoorDecayTauSeconds, 0.075));
    }
    if (last_local >= 0.0 && last_local < DoorOtherToneSeconds) {
        mix += 0.65 * static_cast<double>(smoothTone(
            WarningSecondToneHz, last_local, DoorOtherToneSeconds,
            0.035, DoorDecayTauSeconds, 0.075));
    }
    return clampAudio(mix * 0.82);
}

void FordChimeAudio::renderAudio(float* stream, std::size_t frame_count, int channels) noexcept {
    if (stream == nullptr || channels <= 0 || obtained_.freq <= 0) return;

    const std::uint32_t warning_seq = warning_sequence_.load(std::memory_order_acquire);
    if (warning_seq != seen_warning_sequence_) {
        seen_warning_sequence_ = warning_seq;
        warning_start_sample_ = std::max(sample_clock_, audio_ready_sample_);
        warning_active_ = true;
        door_tone_active_ = false; // hard/soft warning path has priority
    }

    const std::uint32_t door_seq = door_sequence_.load(std::memory_order_acquire);
    if (door_seq != seen_door_sequence_) {
        seen_door_sequence_ = door_seq;
        door_tone_start_sample_ = std::max(sample_clock_, audio_ready_sample_);
        next_door_sample_ = door_tone_start_sample_ + secondsToSamples(DoorPeriodSeconds, obtained_.freq);
        door_tone_active_ = true;
    }

    const std::uint64_t warning_length = secondsToSamples(WarningSequenceSeconds, obtained_.freq);
    const std::uint64_t door_length = secondsToSamples(DoorSequenceSeconds, obtained_.freq);
    const std::uint64_t door_period = secondsToSamples(DoorPeriodSeconds, obtained_.freq);
    const bool door_open = door_open_.load(std::memory_order_acquire);
    const float volume = volume_.load(std::memory_order_acquire);

    for (std::size_t frame = 0U; frame < frame_count; ++frame, ++sample_clock_) {
        float value = 0.0F;

        if (warning_active_) {
            if (sample_clock_ < warning_start_sample_) {
                value = 0.0F;
            } else {
                const std::uint64_t rel = sample_clock_ - warning_start_sample_;
                if (rel < warning_length) {
                    value = renderWarningSample(rel);
                } else {
                warning_active_ = false;
                if (door_open) {
                    door_tone_start_sample_ = sample_clock_;
                    next_door_sample_ = sample_clock_ + door_period;
                    door_tone_active_ = true;
                }
                }
            }
        }

        if (!warning_active_ && door_open) {
            if (!door_tone_active_ && sample_clock_ >= next_door_sample_) {
                door_tone_start_sample_ = sample_clock_;
                next_door_sample_ = sample_clock_ + door_period;
                door_tone_active_ = true;
            }
            if (door_tone_active_) {
                if (sample_clock_ < door_tone_start_sample_) {
                    value = 0.0F;
                } else {
                    const std::uint64_t rel = sample_clock_ - door_tone_start_sample_;
                    if (rel < door_length) value = renderDoorSample(rel);
                    else door_tone_active_ = false;
                }
            }
        } else if (!door_open) {
            door_tone_active_ = false;
        }

        value *= volume;
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t index = frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel);
            stream[index] = value;
        }
    }
}

} // namespace fordadas
