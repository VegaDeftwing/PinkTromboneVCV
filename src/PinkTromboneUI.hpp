#pragma once
#include "PinkTromboneGeometry.hpp"
#include "PinkComponents.hpp"

// NanoVG port of the anatomical drawing and polar touch mapping from:
// https://github.com/evykassirer/pink-trombone/blob/master/lib/tract-ui.js
// and the voicebox control in lib/glottis.js. Copyright 2017 Neil Thapen, MIT.
// The supplemental Rack controls retain every original VCV parameter and CV port.
namespace pinkUI {
struct Row { const char* label; int value, amount, input; };
const Row rows[] = {
    {"Tongue X", PinkTrombone::TONGUEX_PARAM, PinkTrombone::TONGUEXA_PARAM, PinkTrombone::TONGUEXI_INPUT},
    {"Tongue Y", PinkTrombone::TONGUEY_PARAM, PinkTrombone::TONGUEYA_PARAM, PinkTrombone::TONGUEYI_INPUT},
    {"Throat X", PinkTrombone::THROATX_PARAM, PinkTrombone::THROATXA_PARAM, PinkTrombone::THROATXI_INPUT},
    {"Throat Y", PinkTrombone::THROATY_PARAM, PinkTrombone::THROATYA_PARAM, PinkTrombone::THROATYI_INPUT},
    {"Noise Fc", PinkTrombone::FRICFC_PARAM, PinkTrombone::FRICFCA_PARAM, PinkTrombone::FRICFCI_INPUT},
    {"Noise Q", PinkTrombone::FRICQ_PARAM, PinkTrombone::FRICQA_PARAM, PinkTrombone::FRICQI_INPUT},
    {"Level", PinkTrombone::FRICL_PARAM, PinkTrombone::FRICLA_PARAM, PinkTrombone::FRICL_INPUT},
    {"Tenseness", PinkTrombone::TENSE_PARAM, PinkTrombone::TENSEA_PARAM, PinkTrombone::TENSEI_INPUT},
    {"Pitch", PinkTrombone::MASTERPITCH_PARAM, PinkTrombone::VOCTA_PARAM, PinkTrombone::VOCT_INPUT},
    {"Linear FM", -1, PinkTrombone::FMA_PARAM, PinkTrombone::FMI_INPUT},
    {"Vibrato", PinkTrombone::VIB_PARAM, PinkTrombone::VIBA_PARAM, PinkTrombone::VIBI_INPUT}
};
const Row extras[] = {
    {"Nose", PinkTrombone::NOSE_PARAM, PinkTrombone::NOSEA_PARAM, PinkTrombone::NOSE_INPUT},
    {"Tip", PinkTrombone::TIP_PARAM, PinkTrombone::TIPA_PARAM, PinkTrombone::TIP_INPUT},
    {"Blade", PinkTrombone::BLADE_PARAM, PinkTrombone::BLADEA_PARAM, PinkTrombone::BLADE_INPUT},
    {"FC Follow", PinkTrombone::FCFOLLOW_PARAM, PinkTrombone::FCFOLLOWA_PARAM, PinkTrombone::FCFOLLOW_INPUT}
};
// Display pitch/FM/vibrato first without changing saved parameter, port or
// spread IDs used by existing patches.
const int rowOrder[] = {8, 9, 10, 0, 1, 2, 3, 4, 5, 6, 7, 11, 12, 13, 14};
inline const Row& rowAt(int position) {
    int index = rowOrder[position];
    return index < 11 ? rows[index] : extras[index - 11];
}
inline bool polyphonic(PinkTrombone* module) {
    return module && module->display.channels.load(std::memory_order_relaxed) > 1;
}
struct SpreadKnob : Knob {
    void step() override {
        visible = polyphonic(dynamic_cast<PinkTrombone*>(module));
        Knob::step();
    }
};

struct TractPad : widget::OpaqueWidget {
    PinkTrombone* module = nullptr;
    enum Mode { NONE, TONGUE, CONSTRICTION, VOICEBOX } mode = NONE;
    static constexpr float zoom = .54f;
    Vec mouse;
    int ids[2] = {};
    float oldValues[2] = {};
    bool latch = false;

    void pathPoint(NVGcontext* vg, float index, float diameter, bool move = false) {
        auto p = pinkGeometry::point(index, diameter);
        if (move) nvgMoveTo(vg, p.x, p.y); else nvgLineTo(vg, p.x, p.y);
    }
    void label(NVGcontext* vg, float index, float diameter, const char* value, float size = 19.f, NVGcolor color = ink) {
        auto p = pinkGeometry::point(index, diameter);
        nvgSave(vg); nvgTranslate(vg, p.x, p.y);
        nvgRotate(vg, -.24f + index * .64f * M_PI / 38.f - M_PI / 2);
        text(vg, 0, 0, value, size, color, NVG_ALIGN_CENTER);
        nvgRestore(vg);
    }
    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        nvgSave(vg);
        auto font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
        if (font) nvgFontFaceId(vg, font->handle);
        nvgIntersectScissor(vg, 0, 0, box.size.x, box.size.y); nvgScale(vg, zoom, zoom);
        float mouth[44], nose[28], amplitude[44], nasalAmplitude[28];
        int noseStart = 17, blade = 9, tip = 34, channels = 1;
        float tx = .5f, ty = .5f, cx = .06f, cy = .69125f, pitchValue = dsp::FREQ_C4, tense = .44f;
        for (int i = 0; i < 44; ++i) { mouth[i] = i < 7 ? .6f : i < 12 ? 1.1f : 1.5f; amplitude[i] = 0; }
        for (int i = 0; i < 28; ++i) { float d = 2.f * i / 28.f; nose[i] = i == 0 ? .01f : std::min(1.9f, d < 1 ? .4f + 1.6f * d : .5f + 1.5f * (2 - d)); nasalAmplitude[i] = 0; }
        if (module) {
            auto& d = module->display;
            for (int i = 0; i < 44; ++i) { mouth[i] = d.mouth[i].load(std::memory_order_relaxed); amplitude[i] = d.mouthAmplitude[i].load(std::memory_order_relaxed); }
            for (int i = 0; i < 28; ++i) { nose[i] = d.nose[i].load(std::memory_order_relaxed); nasalAmplitude[i] = d.noseAmplitude[i].load(std::memory_order_relaxed); }
            noseStart = d.noseStart.load(); blade = d.bladeStart.load(); tip = d.tipStart.load(); channels = d.channels.load();
            tx = d.tongueX.load(); ty = d.tongueY.load(); cx = d.throatX.load(); cy = d.throatY.load();
            pitchValue = d.pitch.load(); tense = d.tenseness.load();
        }
        float lower = blade + 2, upper = tip - 3, center = (lower + upper) * .5f;
        // Original tongue-control fan and landmark dots.
        nvgBeginPath(vg); pathPoint(vg, lower, 2.05f, true);
        for (float i = lower + 1; i <= upper; ++i) pathPoint(vg, i, 2.05f);
        pathPoint(vg, center, 3.5f); nvgClosePath(vg);
        nvgFillColor(vg, pale); nvgFill(vg); nvgStrokeColor(vg, pale); nvgStrokeWidth(vg, 45); nvgStroke(vg);
        for (int i = -2; i <= 2; ++i) {
            auto p = pinkGeometry::point(center + i * (upper - lower) / 4.f, 2.05f);
            nvgBeginPath(vg); nvgCircle(vg, p.x, p.y, 3); nvgFillColor(vg, nvgRGBA(218,112,214,90)); nvgFill(vg);
        }
        // Filled oral and nasal cavities, using the physical model's live shape.
        nvgBeginPath(vg); pathPoint(vg, 1, 0, true);
        for (int i = 1; i < 44; ++i) pathPoint(vg, i, mouth[i]);
        for (int i = 43; i >= 1; --i) pathPoint(vg, i, 0);
        nvgClosePath(vg); nvgFillColor(vg, pink); nvgFill(vg);
        nvgBeginPath(vg); pathPoint(vg, noseStart, -.8f, true);
        for (int i = 1; i < 28; ++i) pathPoint(vg, i + noseStart, -.8f - nose[i] * .9f);
        for (int i = 27; i >= 0; --i) pathPoint(vg, i + noseStart, -.8f);
        nvgClosePath(vg); nvgFill(vg);
        float velum = nose[0] * 4.f;
        nvgBeginPath(vg); pathPoint(vg, noseStart - 2, 0, true); pathPoint(vg, noseStart, -.8f);
        pathPoint(vg, noseStart + velum, -.8f); pathPoint(vg, noseStart + velum - 2, 0);
        nvgClosePath(vg); nvgFill(vg);
        // Moving cross-sections replace the browser's amplitude strokes.
        nvgStrokeColor(vg, nvgRGBA(192,112,198,90));
        for (int i = 2; i < 43; ++i) {
            nvgBeginPath(vg); pathPoint(vg, i, 0, true); pathPoint(vg, i, mouth[i]);
            nvgStrokeWidth(vg, std::min(8.f, .8f + std::sqrt(amplitude[i]) * 3.f)); nvgStroke(vg);
        }
        for (int i = 1; i < 27; ++i) {
            nvgBeginPath(vg); pathPoint(vg, noseStart + i, -.8f, true); pathPoint(vg, noseStart + i, -.8f - nose[i] * .9f);
            nvgStrokeWidth(vg, std::min(8.f, .8f + std::sqrt(nasalAmplitude[i]) * 3.f)); nvgStroke(vg);
        }
        nvgStrokeColor(vg, ink); nvgStrokeWidth(vg, 4); nvgLineCap(vg, NVG_ROUND); nvgLineJoin(vg, NVG_ROUND);
        nvgBeginPath(vg); pathPoint(vg, 1, mouth[0], true);
        for (int i = 2; i < 44; ++i) pathPoint(vg, i, mouth[i]);
        pathPoint(vg, 1, 0, true); for (int i = 2; i <= noseStart - 2; ++i) pathPoint(vg, i, 0);
        pathPoint(vg, noseStart + velum - 2, 0, true); for (int i = noseStart + int(std::ceil(velum)) - 2; i < 44; ++i) pathPoint(vg, i, 0);
        pathPoint(vg, noseStart, -.8f, true); for (int i = 1; i < 28; ++i) pathPoint(vg, i + noseStart, -.8f - nose[i] * .9f);
        pathPoint(vg, noseStart + velum, -.8f, true); for (int i = int(std::ceil(velum)); i < 28; ++i) pathPoint(vg, i + noseStart, -.8f);
        nvgStroke(vg);
        label(vg, 4.4f, .425f, "throat", 19, nvgRGB(255,255,255));
        label(vg, 26.4f, .9f, "oral", 21, nvgRGB(255,255,255)); label(vg, 31, .9f, "cavity", 21, nvgRGB(255,255,255));
        label(vg, 31, -1.8f, "nasal", 19, nvgRGB(255,255,255)); label(vg, 31, -1.3f, "cavity", 19, nvgRGB(255,255,255));
        label(vg, 19.4f, -.28f, "soft", 17); label(vg, 22.4f, -.28f, "palate", 17);
        label(vg, 33.9f, -.28f, "hard", 17); label(vg, 37.f, -.28f, "palate", 17); label(vg, 42.f, -.28f, "lip", 17);
        label(vg, 45.3f, -1.07f, "nasals", 16); label(vg, 45.3f, -.28f, "stops", 16); label(vg, 45.3f, .51f, "fricatives", 16);
        auto tongue = pinkGeometry::point(lower + tx * (upper - lower), 2.05f + ty * 1.45f);
        nvgBeginPath(vg); nvgCircle(vg, tongue.x, tongue.y, 18); nvgStrokeColor(vg, orchid); nvgStrokeWidth(vg, 4); nvgStroke(vg);
        nvgFillColor(vg, nvgRGBA(218,112,214,35)); nvgFill(vg);
        auto throat = pinkGeometry::point(cx * 44, cy * 4 - 2);
        nvgBeginPath(vg); nvgCircle(vg, throat.x, throat.y, 9); nvgStrokeWidth(vg, 2); nvgStroke(vg);
        text(vg, 180, 419, "tongue control", 18, ink, NVG_ALIGN_CENTER);
        // Original two-band voicebox keyboard: X=pitch, Y=tenseness.
        for (auto bar : {std::pair<float,float>(500, 40), {552, 20}}) {
            nvgBeginPath(vg); nvgRoundedRect(vg, 4, bar.first, 592, bar.second, 8); nvgFillColor(vg, pale); nvgFill(vg);
        }
        for (int i = 0; i < 20; ++i) {
            float x = (i + .5f) * 30;
            nvgBeginPath(vg); nvgMoveTo(vg, x, 509); nvgLineTo(vg, x, 531); nvgMoveTo(vg, x, 558); nvgLineTo(vg, x, 566);
            nvgStrokeColor(vg, nvgRGBA(218,112,214,(i % 12 == 3 || i % 12 == 8 || i % 12 == 10) ? 120 : 65)); nvgStrokeWidth(vg, 3); nvgStroke(vg);
        }
        float px = 600.f * rack::clamp((std::log2(std::max(2.f, pitchValue) / 87.3071f) * 12.f - .5f) / 20.f, 0.f, 1.f);
        float py = 510.f + (1.f - 2.f * std::acos(1.f - rack::clamp(tense, 0.f, 1.f)) / float(M_PI)) * 72.f;
        nvgBeginPath(vg); nvgRoundedRect(vg, px - 9, py - 15, 18, 30, 2); nvgStrokeColor(vg, orchid); nvgStrokeWidth(vg, 3); nvgStroke(vg);
        text(vg, 300, 486, "voicebox control", 18, ink, NVG_ALIGN_CENTER); text(vg, 300, 592, "pitch  <---------------------->", 16, ink, NVG_ALIGN_CENTER);
        text(vg, 530, 469, (std::to_string(channels) + " voice" + (channels == 1 ? "" : "s")).c_str(), 17, ink, NVG_ALIGN_CENTER);
        nvgRestore(vg);
    }

    void updateGesture() {
        if (!module || mode == NONE) return;
        float a = 0.f, b = 0.f;
        if (mode == VOICEBOX) {
            float semitone = 20.f * rack::clamp(mouse.x / 600.f, 0.f, 1.f) + .5f;
            a = std::log2(87.3071f / dsp::FREQ_C4) + semitone / 12.f;
            float t = rack::clamp(1.f - (mouse.y - 510.f) / 72.f, 0.f, 1.f);
            b = 1.f - std::cos(t * M_PI * .5f);
        } else if (mode == TONGUE) {
            float lower = module->display.bladeStart.load() + 2.f, upper = module->display.tipStart.load() - 3.f;
            float diameter = rack::clamp(pinkGeometry::diameter(mouse), 2.05f, 3.5f);
            float fromPoint = (3.5f - diameter) / 1.45f;
            fromPoint = std::pow(fromPoint, .58f) - .2f * (fromPoint * fromPoint - fromPoint);
            float center = (lower + upper) * .5f, range = fromPoint * .5f * (upper - lower);
            float index = rack::clamp(pinkGeometry::index(mouse), center - range, center + range);
            a = (index - lower) / (upper - lower);
            b = (diameter - 2.05f) / 1.45f;
        } else {
            a = pinkGeometry::index(mouse) / 44.f;
            b = (pinkGeometry::diameter(mouse) + 2.f) / 4.f;
        }
        module->paramQuantities[ids[0]]->setValue(a);
        module->paramQuantities[ids[1]]->setValue(b);
    }
    void onButton(const ButtonEvent& e) override {
        if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT || e.action != GLFW_PRESS) { Widget::onButton(e); return; }
        mouse = e.pos.div(zoom);
        float index = pinkGeometry::index(mouse), diameter = pinkGeometry::diameter(mouse);
        float lower = module->display.bladeStart.load() + 2.f, upper = module->display.tipStart.load() - 3.f;
        if (mouse.y >= 500) { mode = VOICEBOX; ids[0] = PinkTrombone::MASTERPITCH_PARAM; ids[1] = PinkTrombone::TENSE_PARAM; }
        else if (index >= lower - 4 && index <= upper + 4 && diameter >= 1.55f && diameter <= 4.f) { mode = TONGUE; ids[0] = PinkTrombone::TONGUEX_PARAM; ids[1] = PinkTrombone::TONGUEY_PARAM; }
        else if (index >= 0 && index < 45 && diameter >= -1.7f && diameter <= 2.f) { mode = CONSTRICTION; ids[0] = PinkTrombone::THROATX_PARAM; ids[1] = PinkTrombone::THROATY_PARAM; }
        else { Widget::onButton(e); return; }
        latch = (e.mods & GLFW_MOD_SHIFT) != 0;
        for (int i = 0; i < 2; ++i) oldValues[i] = module->paramQuantities[ids[i]]->getValue();
        module->voiceboxTouched.store(mode == VOICEBOX, std::memory_order_relaxed);
        updateGesture();
        e.consume(this);
    }
    void onDragMove(const DragMoveEvent& e) override {
        mouse = mouse.plus(e.mouseDelta.div(getAbsoluteZoom() * zoom));
        updateGesture();
    }
    void onDragEnd(const DragEndEvent& e) override {
        if (!module || mode == NONE) return;
        module->voiceboxTouched.store(false, std::memory_order_relaxed);
        if (mode == CONSTRICTION && !latch) {
            for (int i = 0; i < 2; ++i) module->paramQuantities[ids[i]]->setValue(oldValues[i]);
        } else {
            auto* action = new history::ComplexAction;
            action->name = "move Pink Trombone gesture";
            for (int i = 0; i < 2; ++i) {
                float value = module->paramQuantities[ids[i]]->getValue();
                if (value == oldValues[i]) continue;
                auto* change = new history::ParamChange;
                change->moduleId = module->id; change->paramId = ids[i];
                change->oldValue = oldValues[i]; change->newValue = value;
                action->push(change);
            }
            if (!action->isEmpty()) APP->history->push(action); else delete action;
        }
        mode = NONE;
    }
};

struct Panel : Widget {
    PinkTrombone* module = nullptr;
    void draw(const DrawArgs& args) override {
        auto* vg = args.vg;
        nvgBeginPath(vg); nvgRect(vg, 0, 0, 645, 380); nvgFillColor(vg, nvgRGB(255,253,254)); nvgFill(vg);
        nvgBeginPath(vg); nvgRoundedRect(vg, 347, 6, 286, 368, 7); nvgFillColor(vg, pale); nvgFill(vg);
        auto font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
        if (font) nvgFontFaceId(vg, font->handle);
        text(vg, 13, 18, "P I N K   T R O M B O N E", 16, ink);
        text(vg, 365, 19, "CONTROL", 9);
        text(vg, 463, 19, module && module->display.speechActive.load() ? "OFFSET" : "BASE", 9, textColor, NVG_ALIGN_CENTER);
        text(vg, 510, 19, "CV", 9, textColor, NVG_ALIGN_CENTER);
        text(vg, 557, 19, "AMOUNT", 9, textColor, NVG_ALIGN_CENTER);
        if (polyphonic(module)) text(vg, 604, 19, "SPREAD", 9, textColor, NVG_ALIGN_CENTER);
        for (int i = 0; i < 15; ++i) {
            auto& row = rowAt(i);
            text(vg, 365, 43 + 21 * i, row.label);
        }
        text(vg, 584, 364, polyphonic(module) ? "POLY OUT" : "OUT", 9, textColor, NVG_ALIGN_RIGHT);
        text(vg, 14, 105, "VOICE", 7, ink, NVG_ALIGN_CENTER);
        text(vg, 14, 167, "NOISE", 7, ink, NVG_ALIGN_CENTER);
        text(vg, 13, 368, "Drag tongue / voicebox. Shift-drag tract to hold.", 10);

    }
};
}

struct PinkTromboneWidget : ModuleWidget {
    explicit PinkTromboneWidget(PinkTrombone* module) {
        setModule(module);
        box.size = Vec(645, RACK_GRID_HEIGHT);
        auto* panel = new pinkUI::Panel;
        panel->module = module;
        panel->box.size = box.size;
        addChild(panel);
        auto* tract = new pinkUI::TractPad;
        tract->module = module;
        tract->box.pos = Vec(8, 30);
        tract->box.size = Vec(324, 324);
        addChild(tract);
        for (int i = 0; i < 15; ++i) {
            auto& row = pinkUI::rowAt(i);
            float y = 43 + 21 * i;
            addInput(createInputCentered<pinkUI::Jack>(Vec(510, y), module, row.input));
            addParam(createParamCentered<pinkUI::Knob>(Vec(557, y), module, row.amount));
            if (row.value >= 0) addParam(createParamCentered<pinkUI::Knob>(Vec(463, y), module, row.value));
            addParam(createParamCentered<pinkUI::SpreadKnob>(Vec(604, y), module, PinkTrombone::SPREAD_PARAM + pinkUI::rowOrder[i]));
        }
        for (int i = 0; i < 2; ++i) {
            auto* button = createParam<pinkUI::ToggleButton>(Vec(8 + 460 * .54f, 30 + (398 + 34 * i) * .54f),
                module, i == 0 ? PinkTrombone::ALWAYS_VOICE_PARAM : PinkTrombone::PITCH_WOBBLE_PARAM);
            button->label = i == 0 ? "always voice" : "pitch wobble";
            addParam(button);
        }
        addInput(createInputCentered<pinkUI::Jack>(Vec(14, 121), module, PinkTrombone::EXCITATION_INPUT));
        addInput(createInputCentered<pinkUI::Jack>(Vec(14, 183), module, PinkTrombone::NOISE_INPUT));
        addOutput(createOutputCentered<pinkUI::Jack>(Vec(604, 364), module, PinkTrombone::OUTPUT_OUTPUT));
    }
};
