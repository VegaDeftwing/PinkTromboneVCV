#pragma once
#include "../PinkTromboneIds.hpp"
#include <array>
#include <cstdint>
#include <type_traits>

namespace pinkSpeech {
constexpr uint32_t messageMagic = 0x50545350;
constexpr int controlCount = 15;
constexpr int maxVoices = 16;
constexpr int maxTextBytes = 8192;
constexpr int maxSegments = 4096;

// Same stable order as the main module's spread parameters.
const int baseParams[controlCount] = {
    PinkTromboneIds::TONGUEX_PARAM, PinkTromboneIds::TONGUEY_PARAM,
    PinkTromboneIds::THROATX_PARAM, PinkTromboneIds::THROATY_PARAM,
    PinkTromboneIds::FRICFC_PARAM, PinkTromboneIds::FRICQ_PARAM,
    PinkTromboneIds::FRICL_PARAM, PinkTromboneIds::TENSE_PARAM,
    PinkTromboneIds::MASTERPITCH_PARAM, PinkTromboneIds::FMA_PARAM,
    PinkTromboneIds::VIB_PARAM, PinkTromboneIds::NOSE_PARAM,
    PinkTromboneIds::TIP_PARAM, PinkTromboneIds::BLADE_PARAM,
    PinkTromboneIds::FCFOLLOW_PARAM
};
const float neutralKnobs[controlCount] = {
    0.f, 0.f, .06f, .69125f, .5f, .5f, .7325f, .44f, 0.f, 0.f,
    .35f, 4.1f, 2.2f, .87f, 0.f
};
inline int controlForParam(int param) {
    for (int i = 0; i < controlCount; ++i) if (baseParams[i] == param) return i;
    return -1;
}
struct Pose {
    std::array<float, controlCount> controls{{.5f, .5f, 1.f, 1.f, .5f, .5f, 1.f, .65f, -1.125f, 0.f, .06f, 4.1f, 3.f, .9f, 0.f}};
    float voicing = 0.f;
    float noise = .04f;
    float gain = 0.f;
};
struct Message {
    uint32_t magic = messageMagic;
    uint32_t version = 1;
    int channels = 1;
    std::array<Pose, maxVoices> voices;
};
static_assert(std::is_trivially_copyable<Message>::value, "Expander messages must not own resources");
}
