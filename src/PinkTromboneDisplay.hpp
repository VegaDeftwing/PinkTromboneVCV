#pragma once
#include <array>
#include <atomic>
#include <algorithm>

// The audio thread publishes only display data. The UI never reads mutable
// waveguide/voice buffers, and the audio thread never takes a graphics lock.
struct PinkTromboneDisplay {
    std::array<std::atomic<float>, 44> mouth, mouthAmplitude;
    std::array<std::atomic<float>, 28> nose, noseAmplitude;
    std::atomic<bool> speechActive{false};
    std::atomic<float> tongueX{0.f}, tongueY{0.f}, throatX{.06f}, throatY{.69125f};
    std::atomic<float> pitch{261.6256f}, tenseness{.44f};
    std::atomic<int> noseStart{17}, bladeStart{9}, tipStart{34}, channels{1};
    PinkTromboneDisplay() {
        for (int i = 0; i < 44; ++i) { mouth[i] = 1.5f; mouthAmplitude[i] = 0.f; }
        for (int i = 0; i < 28; ++i) {
            float d = 2.f * i / 28.f;
            nose[i] = i == 0 ? .01f : std::min(1.9f, d < 1.f ? .4f + 1.6f * d : .5f + 1.5f * (2.f - d));
            noseAmplitude[i] = 0.f;
        }
    }
};
