#pragma once
#include "PinkComponents.hpp"

namespace pinkSpeechUI {
struct Text : app::LedDisplayTextField {
    PinkSpeech* module = nullptr;
    bool syncing = false;
    Text() {
        multiline = true;
        color = pinkUI::textColor; bgColor = nvgRGB(255,250,252);
        fontPath = asset::system("res/fonts/DejaVuSans.ttf");
        placeholder = "Type ordinary English here...";
    }
    void onChange(const ChangeEvent& event) override {
        if (module && !syncing) module->compiler.request(text);
        LedDisplayTextField::onChange(event);
    }
    void onSelectKey(const SelectKeyEvent& event) override {
        if (module && event.action == GLFW_PRESS && event.key == GLFW_KEY_ENTER && (event.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
            module->requestPlay();
            event.consume(this); return;
        }
        LedDisplayTextField::onSelectKey(event);
    }
    void step() override {
        if (module) {
            auto info = module->compiler.info();
            if (text != info.text) { syncing = true; setText(info.text); syncing = false; }
        }
        LedDisplayTextField::step();
    }
};
struct Panel : Widget {
    PinkSpeech* module = nullptr;
    void draw(const DrawArgs& args) override {
        auto* vg = args.vg;
        nvgBeginPath(vg); nvgRect(vg,0,0,240,380); nvgFillColor(vg,nvgRGB(255,253,254)); nvgFill(vg);
        auto font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
        if (font) nvgFontFaceId(vg,font->handle);
        pinkUI::text(vg,12,19,"P I N K   S P E E C H",15,pinkUI::ink);
        pinkUI::text(vg,12,154,"English text  /  Ctrl+Enter to speak",9);
        std::string status = "Type a sentence to begin";
        float progress = 0.f;
        bool connected = false;
        if (module) {
            auto info = module->compiler.info();
            connected = module->connected.load(); progress = module->progress.load();
            if (!info.error.empty()) status = info.error;
            else if (info.loading) status = "Preparing speech...";
            else if (module->speaking.load()) {
                int word = module->currentWord.load();
                status = word ? "Word " + std::to_string(word) + "/" + std::to_string(module->wordCount.load()) : "Speaking";
                status += "  " + std::to_string(int(progress * 100)) + "%";
            }
            else if (info.sounds) status = "Ready";
            else status = "Enter some English text";
        }
        if (status.size() > 35) status = status.substr(0,32) + "...";
        pinkUI::text(vg,12,208,status.c_str(),10,pinkUI::ink);
        nvgBeginPath(vg); nvgRoundedRect(vg,12,217,216,3,1.5f); nvgFillColor(vg,pinkUI::pale); nvgFill(vg);
        nvgBeginPath(vg); nvgRoundedRect(vg,12,217,216*rack::clamp(progress,0.f,1.f),3,1.5f); nvgFillColor(vg,pinkUI::ink); nvgFill(vg);
        const char* knobLabels[] = {"RATE", "PITCH", "VOICES", "SMOOTH"};
        for (int i=0;i<4;++i) pinkUI::text(vg,30+60*i,234,knobLabels[i],9,pinkUI::textColor,NVG_ALIGN_CENTER);
        const char* triggers[] = {"SPEAK", "WORD", "SELECT", "STOP"};
        for (int i=0;i<4;++i) pinkUI::text(vg,30+60*i,277,triggers[i],9,pinkUI::textColor,NVG_ALIGN_CENTER);
        const char* bottom[] = {"RATE", "V/OCT", "SMOOTH", "GATE", "END"};
        for (int i=0;i<5;++i) pinkUI::text(vg,24+48*i,326,bottom[i],9,pinkUI::textColor,NVG_ALIGN_CENTER);
        pinkUI::text(vg,12,361,connected?"HOST CONNECTED":"Place left of Pink Trombone",9,pinkUI::ink);
    }
};
}
struct PinkSpeechWidget : ModuleWidget {
    explicit PinkSpeechWidget(PinkSpeech* module) {
        setModule(module); box.size=Vec(240,RACK_GRID_HEIGHT);
        auto* panel=new pinkSpeechUI::Panel; panel->module=module; panel->box.size=box.size; addChild(panel);
        auto* field=new pinkSpeechUI::Text; field->module=module; field->box.pos=Vec(12,32); field->box.size=Vec(216,111);
        if(module) field->text=module->compiler.info().text;
        addChild(field);
        const char* labels[]={"Speak","Stop","Word","Repeat"};
        int buttons[]={PinkSpeech::PLAY_PARAM,PinkSpeech::STOP_PARAM,PinkSpeech::WORD_PARAM,PinkSpeech::LOOP_PARAM};
        for(int i=0;i<4;++i) {
            auto* button=createParam<pinkUI::ToggleButton>(Vec(12+i*56,168),module,buttons[i]);
            button->box.size=Vec(50,24); button->label=labels[i]; button->momentary=i<3; addParam(button);
        }
        int knobs[]={PinkSpeech::RATE_PARAM,PinkSpeech::PITCH_PARAM,PinkSpeech::VOICES_PARAM,PinkSpeech::SMOOTH_PARAM};
        for(int i=0;i<4;++i) addParam(createParamCentered<pinkUI::Knob>(Vec(30+i*60,253),module,knobs[i]));
        int triggers[]={PinkSpeech::PLAY_INPUT,PinkSpeech::WORD_INPUT,PinkSpeech::SELECT_INPUT,PinkSpeech::STOP_INPUT};
        for(int i=0;i<4;++i) addInput(createInputCentered<pinkUI::Jack>(Vec(30+i*60,296),module,triggers[i]));
        int cvs[]={PinkSpeech::RATE_INPUT,PinkSpeech::PITCH_INPUT,PinkSpeech::SMOOTH_INPUT};
        for(int i=0;i<3;++i) addInput(createInputCentered<pinkUI::Jack>(Vec(24+i*48,345),module,cvs[i]));
        addOutput(createOutputCentered<pinkUI::Jack>(Vec(168,345),module,PinkSpeech::GATE_OUTPUT));
        addOutput(createOutputCentered<pinkUI::Jack>(Vec(216,345),module,PinkSpeech::END_OUTPUT));
    }
};
