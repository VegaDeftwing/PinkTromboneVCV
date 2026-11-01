#include "plugin.hpp"
#include "speech/Compiler.hpp"
#include <array>

struct PinkSpeech : Module {
    enum Params { PLAY_PARAM, STOP_PARAM, LOOP_PARAM, RATE_PARAM, PITCH_PARAM, VOICES_PARAM, SMOOTH_PARAM, WORD_PARAM, NUM_PARAMS };
    enum Inputs { PLAY_INPUT, STOP_INPUT, RATE_INPUT, PITCH_INPUT, WORD_INPUT, SELECT_INPUT, SMOOTH_INPUT, NUM_INPUTS };
    enum Outputs { GATE_OUTPUT, END_OUTPUT, NUM_OUTPUTS };
    pinkSpeech::Compiler compiler;
    pinkSpeech::Plan* plan = nullptr;
    struct Player {
        pinkSpeech::Plan* utterance = nullptr;
        double time = 0., begin = 0., end = 0.;
        size_t nextWord = 0, wordIndex = 0, firstSegment = 0;
        uint64_t sequenceRevision = 0;
        bool wordMode = false, selected = false, smoothReady = false;
        float selection = 0.f, smoothAmount = -1.f, smoothStep = 0.f, smoothAlpha = 1.f;
        pinkSpeech::Pose smoothed;
        size_t cursor = 0;
        bool playing = false, queued = false;
        dsp::SchmittTrigger playTrigger, stopTrigger, wordTrigger;
        dsp::PulseGenerator endPulse;
    };
    std::array<Player, 16> players;
    dsp::SchmittTrigger playButton, stopButton, wordButton;
    std::atomic<bool> connected{false}, speaking{false};
    std::atomic<float> progress{0.f};
    std::atomic<int> currentPhone{pinkSpeech::PAUSE}, currentWord{0}, wordCount{0};
    pinkSpeech::Message frame;
    std::atomic<uint64_t> playRequests{0};
    std::atomic<bool> resetRequested{false};
    uint64_t seenPlayRequests = 0;
    void requestPlay() { playRequests.fetch_add(1, std::memory_order_release); }
    void release(pinkSpeech::Plan* utterance) {
        if (utterance && --utterance->audioReferences == 0) compiler.retire(utterance);
    }
    void stopPlayer(Player& player) {
        release(player.utterance); player.utterance = nullptr;
        player.playing = player.queued = false;
    }

    PinkSpeech() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
        configButton(PLAY_PARAM, "Speak / restart"); configButton(STOP_PARAM, "Stop");
        configSwitch(LOOP_PARAM, 0, 1, 0, "Repeat", {"Off", "On"});
        configParam(RATE_PARAM, .25f, 3.f, 1.f, "Speech rate", "x");
        configParam(PITCH_PARAM, -24.f, 24.f, 0.f, "Speech pitch", " semitones");
        configParam(VOICES_PARAM, 1.f, 16.f, 1.f, "Speech voices");
        paramQuantities[VOICES_PARAM]->snapEnabled = true;
        configParam(SMOOTH_PARAM, 0.f, 1.f, 0.f, "Articulation smoothing", "%", 0.f, 100.f);
        configButton(WORD_PARAM, "Speak next / selected word");
        configInput(WORD_INPUT, "Next / selected word trigger (polyphonic)");
        configInput(SELECT_INPUT, "Word selection (0–10 V: first–last word, sampled on word trigger)");
        configInput(SMOOTH_INPUT, "Articulation smoothing (+10%/V)");
        configInput(PLAY_INPUT, "Speak / restart trigger (polyphonic)");
        configInput(STOP_INPUT, "Stop trigger (polyphonic)");
        configInput(RATE_INPUT, "Speech rate (1 V = +0.2x)");
        configInput(PITCH_INPUT, "Pitch (1 V/oct)");
        configOutput(GATE_OUTPUT, "Speaking gate"); configOutput(END_OUTPUT, "End of word / phrase trigger");
    }
    ~PinkSpeech() { for (auto& player : players) stopPlayer(player); release(plan); }
    void resetSpeech() {
        compiler.request("Hello. This is Pink Trombone.");
        resetRequested.store(true, std::memory_order_release);
    }
    void onReset(const ResetEvent& event) override {
        Module::onReset(event); resetSpeech();
    }
    json_t* dataToJson() override {
        json_t* data = json_object();
        json_object_set_new(data, "text", json_string(compiler.info().text.c_str()));
        return data;
    }
    void dataFromJson(json_t* data) override {
        json_t* text = json_object_get(data, "text");
        if (json_is_string(text)) compiler.request(json_string_value(text));
    }
    void start(Player& player, bool wordMode = false, bool selected = false, float selection = 0.f) {
        stopPlayer(player);
        player.wordMode = wordMode; player.selected = selected; player.selection = selection;
        player.smoothReady = false;
        player.time = player.begin = 0.; player.cursor = player.firstSegment = 0;
        player.playing = plan && !plan->segments.empty() && (!wordMode || !plan->words.empty());
        if (!player.playing) return;
        if (player.sequenceRevision != plan->revision) { player.nextWord = 0; player.sequenceRevision = plan->revision; }
        player.end = plan->duration;
        if (wordMode) {
            size_t count = plan->words.size();
            player.wordIndex = selected ? std::min(count - 1, size_t(rack::clamp(selection, 0.f, 10.f) * .1f * count)) : player.nextWord % count;
            const auto& word = plan->words[player.wordIndex];
            player.time = player.begin = word.start; player.end = word.end;
            player.cursor = player.firstSegment = word.firstSegment;
            // Selection overrides the sequencer without moving its position.
            if (!selected) player.nextWord = (player.wordIndex + 1) % count;
        } else player.nextWord = 0;
        player.utterance = plan; ++plan->audioReferences;
    }
    void trigger(Player& player, bool wordMode, int channel) {
        bool selected = inputs[SELECT_INPUT].isConnected();
        float selection = inputs[SELECT_INPUT].getPolyVoltage(channel);
        if (!std::isfinite(selection)) selection = 0.f;
        if (plan && plan->revision == compiler.revision()) start(player, wordMode, selected, selection);
        else {
            stopPlayer(player); player.queued = true; player.wordMode = wordMode;
            player.selected = selected; player.selection = selection;
        }
    }
    static pinkSpeech::Pose smooth(Player& player, const pinkSpeech::Pose& target, float amount, float sampleTime) {
        if (!player.smoothReady || amount <= 0.f) {
            player.smoothed = target; player.smoothReady = true; return target;
        }
        if (amount != player.smoothAmount || sampleTime != player.smoothStep) {
            player.smoothAmount = amount; player.smoothStep = sampleTime;
            player.smoothAlpha = -std::expm1(-sampleTime / (.25f * amount * amount));
        }
        float alpha = player.smoothAlpha;
        for (int i = 0; i < pinkSpeech::controlCount; ++i) {
            // Keep pitch and amplitude timing independent of articulation lag.
            if (i == 6 || i == 8 || i == 9) player.smoothed.controls[i] = target.controls[i];
            else player.smoothed.controls[i] += alpha * (target.controls[i] - player.smoothed.controls[i]);
        }
        player.smoothed.voicing += alpha * (target.voicing - player.smoothed.voicing);
        player.smoothed.noise += alpha * (target.noise - player.smoothed.noise);
        player.smoothed.gain = target.gain;
        return player.smoothed;
    }
    void publish(Module* host) {
        if (host && host->model == modelPinkTrombone && host->leftExpander.producerMessage) {
            *static_cast<pinkSpeech::Message*>(host->leftExpander.producerMessage) = frame;
            host->leftExpander.messageFlipRequested = true;
        }
    }
    void processBypass(const ProcessArgs& args) override {
        for (auto& player : players) { stopPlayer(player); player.endPulse.reset(); }
        frame = pinkSpeech::Message{};
        speaking.store(false); progress.store(0.f);
        publish(rightExpander.module);
        Module::processBypass(args);
    }
    void process(const ProcessArgs& args) override {
        Module* host = rightExpander.module;
        bool attached = host && host->model == modelPinkTrombone;
        connected.store(attached, std::memory_order_relaxed);
        int channels = std::max(1, std::min(16, int(params[VOICES_PARAM].getValue())));
        for (auto& input : inputs) channels = std::max(channels, input.getChannels());
        uint64_t requests = playRequests.load(std::memory_order_acquire);
        bool play = playButton.process(params[PLAY_PARAM].getValue()) || requests != seenPlayRequests;
        seenPlayRequests = requests;
        bool stop = stopButton.process(params[STOP_PARAM].getValue());
        bool reset = resetRequested.exchange(false, std::memory_order_acquire);
        stop |= reset;
        if (reset) for (auto& player : players) player.nextWord = 0;
        bool word = wordButton.process(params[WORD_PARAM].getValue());
        // Active voices retain their own immutable utterance while new text is
        // compiled. A trigger on one voice never interrupts another voice.
        if (auto* next = compiler.take()) {
            if (next->revision == compiler.revision()) {
                release(plan); plan = next; plan->audioReferences = 1;
                wordCount.store(int(plan->words.size()), std::memory_order_relaxed);
                for (auto& player : players) if (player.queued) start(player, player.wordMode, player.selected, player.selection);
            } else compiler.retire(next);
        }
        bool any = false;
        frame.channels = channels;
        for (int c = 0; c < 16; ++c) {
            auto& player = players[c];
            bool trigger = player.playTrigger.process(inputs[PLAY_INPUT].getPolyVoltage(c));
            bool wordTrigger = player.wordTrigger.process(inputs[WORD_INPUT].getPolyVoltage(c));
            bool halt = player.stopTrigger.process(inputs[STOP_INPUT].getPolyVoltage(c));
            if (c >= channels || !attached || stop || halt) { stopPlayer(player); player.endPulse.reset(); }
            else if (play || trigger) this->trigger(player, false, c);
            else if (word || wordTrigger) this->trigger(player, true, c);
            pinkSpeech::Pose pose;
            pinkSpeech::Phone phone = pinkSpeech::PAUSE;
            if (player.playing && player.utterance) {
                pose = player.utterance->at(player.time, player.cursor, &phone, player.firstSegment);
                if (player.wordMode) {
                    float edge = float(std::min(1., std::min((player.time - player.begin) / .01, (player.end - player.time) / .02)));
                    pose.gain *= std::max(0.f, edge);
                }
                float smoothCV = inputs[SMOOTH_INPUT].getPolyVoltage(c);
                if (!std::isfinite(smoothCV)) smoothCV = 0.f;
                pose = smooth(player, pose, rack::clamp(params[SMOOTH_PARAM].getValue() + smoothCV * .1f, 0.f, 1.f), args.sampleTime);
                float rateCV = inputs[RATE_INPUT].getPolyVoltage(c);
                float pitchCV = inputs[PITCH_INPUT].getPolyVoltage(c);
                if (!std::isfinite(rateCV)) rateCV = 0.f;
                if (!std::isfinite(pitchCV)) pitchCV = 0.f;
                pose.controls[8] += params[PITCH_PARAM].getValue() / 12.f + pitchCV;
                player.time += args.sampleTime * rack::clamp(params[RATE_PARAM].getValue() + rateCV * .2f, .15f, 4.f);
                if (player.time >= player.end) {
                    player.endPulse.trigger(.001f);
                    if (!player.wordMode && params[LOOP_PARAM].getValue() >= .5f) start(player);
                    else stopPlayer(player);
                }
            }
            frame.voices[c] = pose;
            any |= player.playing;
            if (c < channels) {
                outputs[GATE_OUTPUT].setVoltage(player.playing ? 10.f : 0.f, c);
                outputs[END_OUTPUT].setVoltage(player.endPulse.process(args.sampleTime) ? 10.f : 0.f, c);
            }
            if (c == 0) {
                progress.store(player.playing && player.end > player.begin ? float((player.time - player.begin) / (player.end - player.begin)) : 0.f, std::memory_order_relaxed);
                currentPhone.store(phone, std::memory_order_relaxed);
                currentWord.store(player.playing && player.wordMode ? int(player.wordIndex + 1) : 0, std::memory_order_relaxed);
            }
        }
        speaking.store(any, std::memory_order_relaxed);
        outputs[GATE_OUTPUT].setChannels(channels); outputs[END_OUTPUT].setChannels(channels);
        if (attached) publish(host);
    }
};

#include "PinkSpeechUI.hpp"
Model* modelPinkSpeech = createModel<PinkSpeech, PinkSpeechWidget>("PinkSpeech");
