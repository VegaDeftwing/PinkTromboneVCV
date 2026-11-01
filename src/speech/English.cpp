#include "English.hpp"
extern "C" {
#include "flite.h"
#include "usenglish.h"
#include "cmu_lex.h"
}
#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace pinkSpeech {
namespace {
const char* names[phoneCount] = {"pau","aa","ae","ah","ao","aw","ay","b","ch","d","dh","eh","er","ey","f","g","hh","ih","iy","jh","k","l","m","n","ng","ow","oy","p","r","s","sh","t","th","uh","uw","v","w","y","z","zh","ax"};
std::mutex frontendMutex;
struct Frontend {
    cst_voice* voice;
    Frontend() {
        flite_init();
        voice = new_voice();
        usenglish_init(voice);
        feat_set(voice->features, "lexicon", lexicon_val(cmu_lex_init()));
        feat_set(voice->features, "postlex_func", uttfunc_val(cmu_postlex));
        feat_set_float(voice->features, "int_f0_target_mean", 125.f);
        feat_set_float(voice->features, "int_f0_target_stddev", 14.f);
    }
    ~Frontend() { delete_voice(voice); }
};
std::string asciiText(const std::string& text) {
    if (text.size() > maxTextBytes) throw std::runtime_error("Text is too long (8192 bytes maximum)");
    std::string result;
    // Common English typography is normalized rather than exposed as phonetic
    // notation. Unsupported Unicode is skipped safely; English words use LTS.
    for (size_t i = 0; i < text.size();) {
        unsigned char c = text[i];
        if (c < 128) { result += c >= 32 || c == '\n' || c == '\t' ? char(c) : ' '; ++i; continue; }
        if (i + 2 < text.size() && c == 0xe2 && (unsigned char)text[i+1] == 0x80) {
            unsigned char last = text[i+2];
            if (last == 0x98 || last == 0x99) result += '\'';
            else if (last == 0x9c || last == 0x9d) result += '"';
            else result += ' ';
            i += 3; continue;
        }
        if (c == 0xc3 && i + 1 < text.size()) {
            unsigned char a = text[i + 1]; char letter = ' ';
            if ((a >= 0x80 && a <= 0x85) || (a >= 0xa0 && a <= 0xa5)) letter = 'a';
            if ((a >= 0x88 && a <= 0x8b) || (a >= 0xa8 && a <= 0xab)) letter = 'e';
            if ((a >= 0x8c && a <= 0x8f) || (a >= 0xac && a <= 0xaf)) letter = 'i';
            if ((a >= 0x92 && a <= 0x96) || (a >= 0xb2 && a <= 0xb6)) letter = 'o';
            if ((a >= 0x99 && a <= 0x9c) || (a >= 0xb9 && a <= 0xbc)) letter = 'u';
            if (a == 0x87 || a == 0xa7) letter = 'c';
            if (a == 0x91 || a == 0xb1) letter = 'n';
            result += letter; i += 2; continue;
        }
        result += ' '; ++i;
        while (i < text.size() && ((unsigned char)text[i] & 0xc0) == 0x80) ++i;
    }
    return result;
}
void tongue(Pose& p, float index, float diameter) {
    p.controls[0] = std::max(0.f, std::min(1.f, (index - 12.f) / 17.f));
    p.controls[1] = std::max(0.f, std::min(1.f, (diameter - 2.05f) / 1.45f));
}
void constrict(Pose& p, float index, float diameter) { p.controls[2] = index / 44.f; p.controls[3] = (diameter + 2.f) / 4.f; }
bool vowel(Phone p) { return p == AX || p == AA || p == AE || p == AH || p == AO || p == AW || p == AY || p == EH || p == ER || p == EY || p == IH || p == IY || p == OW || p == OY || p == UH || p == UW; }
bool stop(Phone p) { return p == B || p == D || p == G || p == K || p == P || p == T || p == CH || p == JH; }
Pose articulation(Phone phone) {
    Pose p;
    p.gain = phone == PAUSE ? 0.f : 1.f;
    p.voicing = 1.f;
    tongue(p, 20.7f, 2.8f);
    switch (phone) {
    case PAUSE: p.voicing = 0.f; p.noise = 0.f; break;
    // Articulatory coordinates based on the programmable Pink Trombone phoneme
    // reference (zakaton/Pink-Trombone, GPL-3.0); coarticulation is applied below.
    case AA: tongue(p, 12.f, 2.05f); break;
    case AH: tongue(p, 17.8f, 2.46f); break;
    case AX: tongue(p, 20.7f, 2.8f); break;
    case AE: tongue(p, 14.93f, 2.78f); break;
    case AO: tongue(p, 17.7f, 2.05f); break;
    case EH: tongue(p, 19.4f, 3.43f); break;
    case ER: tongue(p, 20.7f, 2.8f); constrict(p, 30.f, 1.15f); break;
    case IH: tongue(p, 26.11f, 2.87f); break;
    case IY: tongue(p, 27.2f, 2.2f); break;
    case UH: tongue(p, 22.5f, 2.5f); break;
    case UW: tongue(p, 22.8f, 2.05f); constrict(p, 42.f, 1.2f); break;
    case AW: case AY: tongue(p, 12.f, 2.05f); break;
    case EY: tongue(p, 19.4f, 3.43f); break;
    case OW: case OY: tongue(p, 17.7f, 2.05f); break;
    case B: case P: constrict(p, 41.f, 0.f); break;
    case D: case T: case CH: case JH: constrict(p, 36.f, 0.f); break;
    case G: case K: constrict(p, 20.f, 0.f); break;
    case M: constrict(p, 41.f, -1.f); break;
    case N: constrict(p, 36.f, -1.f); break;
    case NG: constrict(p, 20.f, -1.f); break;
    case F: case V: constrict(p, 41.f, .5f); p.noise = 1.f; p.controls[4] = 2.f; break;
    case S: case Z: constrict(p, 36.f, .6f); p.noise = 1.f; p.controls[4] = 5.f; break;
    case SH: case ZH: constrict(p, 31.f, .6f); p.noise = 1.f; p.controls[4] = 3.f; break;
    case TH: case DH: constrict(p, 40.f, .65f); p.noise = .8f; p.controls[4] = 3.f; break;
    case HH: p.noise = 1.f; p.controls[4] = 1.5f; break;
    case L: constrict(p, 38.f, 1.1f); break;
    case R: tongue(p, 20.7f, 2.8f); constrict(p, 30.f, 1.f); break;
    case W: tongue(p, 22.8f, 2.05f); constrict(p, 42.f, 1.f); break;
    case Y: tongue(p, 27.2f, 2.2f); break;
    default: break;
    }
    if (phone == P || phone == T || phone == K || phone == F || phone == S || phone == SH || phone == TH || phone == HH || phone == CH) p.voicing = 0.f;
    if (stop(phone)) { p.noise = .015f; p.voicing *= .35f; }
    return p;
}
Pose interpolate(const Pose& a, const Pose& b, float t) {
    Pose p;
    for (int i = 0; i < controlCount; ++i) p.controls[i] = a.controls[i] + (b.controls[i] - a.controls[i]) * t;
    p.voicing = a.voicing + (b.voicing - a.voicing) * t;
    p.noise = a.noise + (b.noise - a.noise) * t;
    p.gain = a.gain + (b.gain - a.gain) * t;
    return p;
}
}
const char* phoneName(Phone phone) { return names[std::max(0, std::min(int(phoneCount) - 1, int(phone)))]; }
Phone phoneFromName(const std::string& name) {
    if (name == "ax") return AX;
    if (name == "axr") return ER;
    if (name == "dx") return D;
    if (name == "el") return L;
    if (name == "em") return M;
    if (name == "en") return N;
    for (int i = 0; i < phoneCount; ++i) if (name == names[i]) return Phone(i);
    return PAUSE;
}
Plan compileEnglish(const std::string& text) {
    Plan plan;
    std::string normalized = asciiText(text);
    if (normalized.find_first_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == std::string::npos) return plan;
    std::lock_guard<std::mutex> lock(frontendMutex);
    static Frontend frontend;
    std::unique_ptr<cst_utterance, void(*)(cst_utterance*)> utterance(flite_synth_text(normalized.c_str(), frontend.voice), delete_utterance);
    if (!utterance) throw std::runtime_error("Could not pronounce this text");
    std::vector<std::pair<float, float>> pitchTargets;
    for (auto* t = relation_head(utt_relation(utterance.get(), "Target")); t; t = item_next(t))
        pitchTargets.emplace_back(item_feat_float(t, "pos"), item_feat_float(t, "f0"));
    auto pitchAt = [&](double time) {
        if (pitchTargets.empty()) return 125.f;
        size_t i = 0;
        while (i + 1 < pitchTargets.size() && pitchTargets[i + 1].first < time) ++i;
        if (i + 1 == pitchTargets.size()) return pitchTargets[i].second;
        auto a = pitchTargets[i], b = pitchTargets[i + 1];
        float t = float(std::max(0., std::min(1., (time - a.first) / std::max(.00001f, b.first - a.first))));
        return a.second + t * (b.second - a.second);
    };
    double start = 0.;
    const cst_item* previousWord = nullptr;
    for (auto* s = relation_head(utt_relation(utterance.get(), "Segment")); s; s = item_next(s)) {
        if (plan.segments.size() >= maxSegments) throw std::runtime_error("Text contains too many speech sounds");
        Segment segment;
        segment.start = start;
        segment.end = std::max(start + .015, double(item_feat_float(s, "end")));
        segment.phone = phoneFromName(item_feat_string(s, "name"));
        segment.word = ffeature_string(s, "R:SylStructure.parent.parent.name");
        if (segment.word == "0") segment.word.clear();
        const cst_item* word = item_parent(item_parent(item_as(s, "SylStructure")));
        if (word && segment.phone != PAUSE) {
            if (word != previousWord) {
                Word range; range.start = segment.start; range.firstSegment = plan.segments.size(); range.text = segment.word;
                plan.words.push_back(range);
            }
            plan.words.back().end = segment.end;
        }
        previousWord = word;
        segment.pose = articulation(segment.phone);
        segment.finish = segment.pose;
        if (segment.phone == AY || segment.phone == EY || segment.phone == OY) tongue(segment.finish, 27.2f, 2.2f);
        if (segment.phone == AW || segment.phone == OW) tongue(segment.finish, 22.8f, 2.05f);
        if (segment.phone == CH || segment.phone == JH) { constrict(segment.finish, 31.f, .6f); segment.finish.noise = 1.f; segment.finish.voicing = segment.phone == CH ? 0.f : 1.f; }
        segment.pose.controls[8] = std::log2(std::max(60.f, pitchAt(start)) / 261.625565f);
        segment.finish.controls[8] = std::log2(std::max(60.f, pitchAt(segment.end)) / 261.625565f);
        plan.segments.push_back(segment);
        if (!plan.pronunciation.empty()) plan.pronunciation += ' ';
        plan.pronunciation += phoneName(segment.phone);
        start = segment.end;
    }
    // Keep consonants on the neighboring vowel's tongue shape, not a repeated
    // neutral vowel. This provides anticipatory coarticulation between words.
    for (size_t i = 0; i < plan.segments.size(); ++i) {
        if (vowel(plan.segments[i].phone) || plan.segments[i].phone == PAUSE) continue;
        size_t nearest = i;
        for (size_t distance = 1; distance < 8; ++distance) {
            if (i + distance < plan.segments.size() && vowel(plan.segments[i + distance].phone)) { nearest = i + distance; break; }
            if (distance <= i && vowel(plan.segments[i - distance].phone)) { nearest = i - distance; break; }
        }
        if (nearest != i) for (int c = 0; c < 2; ++c)
            plan.segments[i].pose.controls[c] = plan.segments[i].finish.controls[c] = plan.segments[nearest].pose.controls[c];
    }
    plan.duration = start;
    return plan;
}
Pose Plan::at(double time, size_t& cursor, Phone* phone, size_t firstSegment) const {
    if (segments.empty() || time < 0. || time >= duration) return Pose{};
    cursor = std::min(cursor, segments.size() - 1);
    while (cursor && segments[cursor].start > time) --cursor;
    while (cursor + 1 < segments.size() && segments[cursor].end <= time) ++cursor;
    const auto& s = segments[cursor];
    if (phone) *phone = s.phone;
    float phase = float((time - s.start) / (s.end - s.start));
    Pose p = interpolate(s.pose, s.finish, std::max(0.f, std::min(1.f, phase)));
    double attack = std::min(stop(s.phone) ? .008 : .018, (s.end - s.start) * .25);
    if (cursor > firstSegment && time - s.start < attack)
    {
        const auto& previous = segments[cursor - 1];
        Pose from = previous.finish;
        // A released stop is open by this point. Do not close it again at the
        // next phone boundary merely because its stored target is a closure.
        if (stop(previous.phone) && previous.phone != CH && previous.phone != JH) { from.controls[3] = 1.f; from.noise = .7f; }
        p = interpolate(from, p, float((time - s.start) / attack));
    }
    // Release a stop into the following sound, producing the model's transient.
    if (stop(s.phone) && s.phone != CH && s.phone != JH && phase > .78f) {
        p.controls[3] = 1.f;
        p.noise = .7f;
    }
    float edge = float(std::min(1., std::min(time / .01, (duration - time) / .02)));
    p.gain *= std::max(0.f, edge);
    return p;
}
}
