#pragma once
#include "plugin.hpp"

namespace pinkUI {
const NVGcolor ink = nvgRGB(192, 112, 198);
const NVGcolor orchid = nvgRGB(218, 112, 214);
const NVGcolor pale = nvgRGB(255, 238, 245);
const NVGcolor pink = nvgRGB(255, 192, 203);
const NVGcolor textColor = nvgRGB(106, 65, 104);
inline void text(NVGcontext* vg, float x, float y, const char* value, float size = 11.f, NVGcolor color = textColor, int align = NVG_ALIGN_LEFT) {
    nvgFontSize(vg, size);
    nvgTextAlign(vg, align | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, color);
    nvgText(vg, x, y, value, nullptr);
}
struct Knob : app::Knob {
    Knob() { box.size = Vec(18, 18); minAngle = -.8f * M_PI; maxAngle = .8f * M_PI; }
    void draw(const DrawArgs& args) override {
        float scaled = getParamQuantity() ? getParamQuantity()->getScaledValue() : .5f;
        float angle = minAngle + (maxAngle - minAngle) * scaled;
        nvgBeginPath(args.vg); nvgCircle(args.vg, 9, 9, 7.5f);
        nvgFillColor(args.vg, nvgRGB(255, 250, 252)); nvgFill(args.vg);
        nvgStrokeColor(args.vg, ink); nvgStrokeWidth(args.vg, 1.2f); nvgStroke(args.vg);
        nvgBeginPath(args.vg); nvgMoveTo(args.vg, 9, 9);
        nvgLineTo(args.vg, 9 + std::sin(angle) * 6, 9 - std::cos(angle) * 6);
        nvgStrokeWidth(args.vg, 1.5f); nvgStroke(args.vg);
    }
};

struct Jack : app::PortWidget {
    Jack() { box.size = Vec(18, 18); }
    void draw(const DrawArgs& args) override {
        auto* vg = args.vg;
        nvgBeginPath(vg); nvgCircle(vg, 9, 9, 8);
        nvgFillColor(vg, nvgRGB(255, 250, 252)); nvgFill(vg);
        nvgStrokeColor(vg, ink); nvgStrokeWidth(vg, 1.3f); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 9, 9, 5);
        nvgFillColor(vg, type == engine::Port::OUTPUT ? ink : nvgRGB(111, 63, 108)); nvgFill(vg);
        nvgBeginPath(vg); nvgCircle(vg, 9, 9, 2.5f);
        nvgFillColor(vg, nvgRGB(52, 31, 52)); nvgFill(vg);
        PortWidget::draw(args);
    }
};

// Real Rack switch parameters: each button owns its state, mouse handling,
// undo and patch serialization, independently of level and vibrato amounts.
struct ToggleButton : app::Switch {
    std::string label;
    ToggleButton() { box.size = Vec(73.44f, 15.12f); }
    void draw(const DrawArgs& args) override {
        auto* vg = args.vg;
        bool enabled = !getParamQuantity() || getParamQuantity()->getValue() >= .5f;
        nvgBeginPath(vg); nvgRoundedRect(vg, .5f, .5f, box.size.x - 1, box.size.y - 1, 4);
        nvgFillColor(vg, enabled ? ink : pale); nvgFill(vg);
        nvgStrokeColor(vg, ink); nvgStrokeWidth(vg, .8f); nvgStroke(vg);
        auto font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
        if (font) nvgFontFaceId(vg, font->handle);
        text(vg, box.size.x * .5f, box.size.y * .5f, label.c_str(), 8.6f,
             enabled ? nvgRGB(255, 250, 252) : ink, NVG_ALIGN_CENTER);
    }
};

}
