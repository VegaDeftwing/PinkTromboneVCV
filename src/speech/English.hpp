#pragma once
#include "SpeechProtocol.hpp"
#include <string>
#include <vector>

namespace pinkSpeech {
enum Phone {
    PAUSE, AA, AE, AH, AO, AW, AY, B, CH, D, DH, EH, ER, EY, F, G,
    HH, IH, IY, JH, K, L, M, N, NG, OW, OY, P, R, S, SH, T, TH, UH, UW,
    V, W, Y, Z, ZH, AX, phoneCount
};
const char* phoneName(Phone phone);
Phone phoneFromName(const std::string& name);
struct Segment {
    double start = 0., end = 0.;
    Pose pose;
    Pose finish;
    Phone phone = PAUSE;
    std::string word;
};
struct Word {
    double start = 0., end = 0.;
    size_t firstSegment = 0;
    std::string text;
};
struct Plan {
    uint64_t revision = 0;
    std::vector<Segment> segments;
    std::vector<Word> words;
    std::string pronunciation;
    double duration = 0.;
    Plan* retiredNext = nullptr;
    // Audio-thread-only references: one for the current plan plus active voices.
    unsigned audioReferences = 0;
    Pose at(double time, size_t& cursor, Phone* phone = nullptr, size_t firstSegment = 0) const;
};
// CMU dictionary + learned letter-to-sound fallback, normalization, duration and
// intonation models. Called only by the compiler worker, never the audio thread.
Plan compileEnglish(const std::string& text);
}
