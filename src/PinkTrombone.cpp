#include "plugin.hpp"
#include "PinkTromboneVoice.hpp"
#include "PinkTromboneDisplay.hpp"
#include <array>
#include <memory>

struct PinkTrombone : Module, PinkTromboneIds {
    std::array<std::unique_ptr<PinkTromboneVoice>, 16> voices;
    dsp::ClockDivider processDivider;
    dsp::ClockDivider lightDivider;
    dsp::ClockDivider displayDivider;
    PinkTromboneDisplay display;
    float sampleRate = 48000.f;
    int previousChannels = 0;
    std::atomic<bool> voiceboxTouched{false};
    uint32_t spreadSeed;
    pinkSpeech::Message speechMessages[2];
    bool speechWasConnected = false;

    void setSpreadSeed(uint32_t seed) {
        spreadSeed = seed;
        for (int c = 0; c < 16; ++c)
            for (int row = 0; row < 15; ++row) {
                uint32_t bits = seed ^ (0x9e3779b9u * (c + 1)) ^ (0x85ebca6bu * (row + 1));
                bits ^= bits >> 16; bits *= 0x7feb352du;
                bits ^= bits >> 15; bits *= 0x846ca68bu; bits ^= bits >> 16;
                voices[c]->spreadOffsets[row] = (bits >> 8) * (2.f / 16777216.f) - 1.f;
            }
    }

    json_t* dataToJson() override {
        json_t* data = json_object();
        json_object_set_new(data, "spreadSeed", json_integer(spreadSeed));
        return data;
    }
    void dataFromJson(json_t* data) override {
        json_t* seed = json_object_get(data, "spreadSeed");
        if (json_is_integer(seed)) setSpreadSeed(uint32_t(json_integer_value(seed)));
    }

    explicit PinkTrombone(uint32_t seed = rack::random::u32()) {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TONGUEX_PARAM, 0.f, 1.f, 0.f, "Tongue X");
		configParam(TONGUEY_PARAM, 0.f, 1.f, 0.f, "Tongue Y");
		configParam(NOSE_PARAM, 1.f, 10.f, 4.1, "Nose");
		configParam(THROATX_PARAM, 0.06, 0.999, 0.06, "Throat X");
		configParam(THROATY_PARAM, 0.f, 1.f, 0.69125, "Throat Y");
		configParam(TONGUEXA_PARAM, -1.f, 1.f, 0.f, "Tongue X Attenuversion");
		configParam(TONGUEYA_PARAM, -1.f, 1.f, 0.f, "Tongue Y Attenuversion");
		configParam(THROATXA_PARAM, -1.f, 1.f, 0.f, "Throat X Attenuversion");
		configParam(THROATYA_PARAM, -1.f, 1.f, 0.f, "Throat Y Attenuversion");
		configParam(TIP_PARAM, 0.f, 9.f, 2.2, "Tip");
		// configParam(LIP_PARAM, 0.f, 9.f, 0.f, "Lip");
		configParam(BLADE_PARAM, 0.f, 1.f, 0.87, "Blade");
		configParam(FRICFC_PARAM, 0.f, 1.f, 0.5, "Fricative & Aspiration Fc");
		configParam(FRICFCA_PARAM, -1.f, 1.f, 0.f, "Fricative & Aspiration Fc Attenuversion");
		configParam(FRICQ_PARAM, 0.00001, 1.2, 0.5, "Fricative & Aspiration Q");
		configParam(FRICQA_PARAM, -1.f, 1.f, 0.f, "Fricative Q Attenuversion");
		configParam(FRICL_PARAM, 0.f, 1.f, 0.7325, "Fricative Level");
		configParam(FRICLA_PARAM, -1.f, 1.f, 0.f, "Fricative Level Attenuversion");
		configParam(FCFOLLOW_PARAM, 0.f, 1.f, 0.f, "Fricative and Aspirate Fc Follow Master Pitch");
		configParam(TENSEA_PARAM, 0.f, 1.f, 0.f, "Tense Attenuversion");
		configParam(TENSE_PARAM, 0.f, 1.f, 0.44, "Tenseness");
		configParam(VOCTA_PARAM, 0.f, 1.f, 1.f, "V/Oct Attenuversion");
		configParam(MASTERPITCH_PARAM, -2.f, 4.f, 0.f, "Master Pitch", " Hz", 2, dsp::FREQ_C4); // range to prevent lockups
		configParam(FMA_PARAM, -1.f, 1.f, 0.f, "FM Attenuversion");
		configParam(VIBA_PARAM, 0.f, 1.f, 0.f, "Vibrato Attenuversion");
		configParam(VIB_PARAM, 0.f, 1.f, 0.35, "Vibrato");
        configParam(NOSEA_PARAM, -1.f, 1.f, 0.f, "Nose CV Amount");
        configParam(TIPA_PARAM, -1.f, 1.f, 0.f, "Tip CV Amount");
        configParam(BLADEA_PARAM, -1.f, 1.f, 0.f, "Blade CV Amount");
        configParam(FCFOLLOWA_PARAM, -1.f, 1.f, 0.f, "Fc Follow CV Amount");
        configSwitch(ALWAYS_VOICE_PARAM, 0.f, 1.f, 1.f, "Always voice", {"Voicebox touch only", "Continuous"});
        configSwitch(PITCH_WOBBLE_PARAM, 0.f, 1.f, 1.f, "Pitch wobble", {"Off", "On"});
        const char* spreadNames[] = {"Tongue X", "Tongue Y", "Throat X", "Throat Y", "Noise Fc", "Noise Q", "Level", "Tenseness", "Pitch", "Linear FM", "Vibrato", "Nose", "Tip", "Blade", "Fc follow"};
        for (int row = 0; row < 15; ++row)
            configParam(SPREAD_PARAM + row, 0.f, 1.f, 0.f, std::string(spreadNames[row]) + " voice spread", "%", 0.f, 100.f);
        configInput(NOSE_INPUT, "Nose");
        configInput(TIP_INPUT, "Tip");
        configInput(BLADE_INPUT, "Blade");
        configInput(FCFOLLOW_INPUT, "Fc Follow");
		configInput(TONGUEXI_INPUT, "Tongue X");
		configInput(TONGUEYI_INPUT, "Tongue Y");
		configInput(THROATXI_INPUT, "Throat X");
		configInput(THROATYI_INPUT, "Throat Y");
		configInput(FRICFCI_INPUT, "Fricative Fc");
		configInput(FRICQI_INPUT, "Fricative Q");
		configInput(FRICL_INPUT, "Fricative Level");
		configInput(TENSEI_INPUT, "Tenseness");
		configInput(VOCT_INPUT, "VOct");
		configInput(FMI_INPUT, "FM");
		configInput(VIBI_INPUT, "Vibrato");
        configInput(EXCITATION_INPUT, "Voice excitation (replaces glottis; mutes unpatched internal noise)");
        configInput(NOISE_INPUT, "Noise excitation (replaces white noise)");
		configOutput(OUTPUT_OUTPUT, "Main");
        configBypass(EXCITATION_INPUT, OUTPUT_OUTPUT);

        // Allocate every voice before audio processing; channel changes and
        // sample-rate events never allocate or construct a physical model.
        for (int c = 0; c < 16; ++c)
            voices[c].reset(new PinkTromboneVoice(sampleRate, seed + 0x9e3779b9u * c));
        setSpreadSeed(seed ^ 0xa511e9b3u);
        leftExpander.producerMessage = &speechMessages[0];
        leftExpander.consumerMessage = &speechMessages[1];
        processDivider.setDivision(24);
        lightDivider.setDivision(48);
        displayDivider.setDivision(800);
    }

    void onSampleRateChange(const SampleRateChangeEvent& event) override {
        sampleRate = event.sampleRate;
        for (auto& voice : voices) voice->setSampleRate(sampleRate);
        lightDivider.setDivision(std::max(1, int(sampleRate / 1000)));
        displayDivider.setDivision(std::max(1, int(sampleRate / 60)));
    }

    void process(const ProcessArgs& args) override {
        if (sampleRate != args.sampleRate)
            onSampleRateChange({args.sampleRate, args.sampleTime});
        int channels = 1;
        for (auto& input : inputs) channels = std::max(channels, input.getChannels());
        const pinkSpeech::Message* speech = nullptr;
        if (leftExpander.module && leftExpander.module->model == modelPinkSpeech) {
            auto* message = static_cast<const pinkSpeech::Message*>(leftExpander.consumerMessage);
            if (message && message->magic == pinkSpeech::messageMagic && message->version == 1) speech = message;
        }
        if (speech) channels = std::max(channels, std::max(1, std::min(16, speech->channels)));
        bool speechChanged = bool(speech) != speechWasConnected;
        speechWasConnected = speech != nullptr;
        display.speechActive.store(speech != nullptr, std::memory_order_relaxed);
        bool controlFrame = processDivider.process();
        bool touched = voiceboxTouched.load(std::memory_order_relaxed);
        for (int c = 0; c < channels; ++c) {
            auto& voice = *voices[c];
            voice.touchActive = touched;
            const pinkSpeech::Pose* pose = speech ? &speech->voices[speech->channels == 1 ? 0 : c] : nullptr;
            if (channels != previousChannels || speechChanged) voice.updateControls(params, inputs, c, channels > 1, pose);
            outputs[OUTPUT_OUTPUT].setVoltage(voice.process(inputs[EXCITATION_INPUT].isConnected(), inputs[EXCITATION_INPUT].getPolyVoltage(c),
                inputs[NOISE_INPUT].isConnected(), inputs[NOISE_INPUT].getPolyVoltage(c), pose), c);
            if (controlFrame) voice.updateControls(params, inputs, c, channels > 1, pose);
        }
        outputs[OUTPUT_OUTPUT].setChannels(channels);
        previousChannels = channels;
        if (lightDivider.process()) voices[0]->updateLights(lights);
        if (displayDivider.process()) {
            auto& voice = *voices[0];
            for (int i = 0; i < 44; ++i) {
                display.mouth[i].store(voice.properties.tractDiameter[i], std::memory_order_relaxed);
                display.mouthAmplitude[i].store(voice.tract.amplitude(i), std::memory_order_relaxed);
            }
            for (int i = 0; i < 28; ++i) {
                display.nose[i].store(voice.properties.noseDiameter[i], std::memory_order_relaxed);
                display.noseAmplitude[i].store(voice.tract.noseAmplitude(i), std::memory_order_relaxed);
            }
            display.tongueX.store(voice.tongueX, std::memory_order_relaxed);
            display.tongueY.store(voice.tongueY, std::memory_order_relaxed);
            display.throatX.store(voice.constrictionX, std::memory_order_relaxed);
            display.throatY.store(voice.constrictionY, std::memory_order_relaxed);
            display.pitch.store(voice.pitch, std::memory_order_relaxed);
            display.tenseness.store(voice.tenseness, std::memory_order_relaxed);
            display.noseStart.store(voice.properties.noseStart, std::memory_order_relaxed);
            display.bladeStart.store(voice.properties.bladeStart, std::memory_order_relaxed);
            display.tipStart.store(voice.properties.tipStart, std::memory_order_relaxed);
            display.channels.store(channels, std::memory_order_relaxed);
        }
    }
};

#include "PinkTromboneUI.hpp"

Model *modelPinkTrombone = createModel<PinkTrombone, PinkTromboneWidget>("PinkTrombone");
