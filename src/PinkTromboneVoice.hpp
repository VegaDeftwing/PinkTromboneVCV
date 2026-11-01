#pragma once
#include "PinkTromboneIds.hpp"
#include "speech/SpeechProtocol.hpp"
#include "PinkTrombone/Glottis.hpp"
#include "PinkTrombone/Tract.hpp"
#include "PinkTrombone/WhiteNoise.hpp"
#include "PinkTrombone/Biquad.hpp"
#include "PinkTrombone/noise.hpp"
#include <cmath>
#include <array>

// One complete physical voice. No model, filter or noise history is shared
// between channels. The small simplex lookup table is immutable after startup.
struct PinkTromboneVoice : PinkTromboneIds {
    struct Properties : t_tractProps {
        Properties() { initializeTractProps(this, 44); }
        ~Properties() { destroyTractProps(this); }
        Properties(const Properties&) = delete;
        Properties& operator=(const Properties&) = delete;
    } properties;

    float sampleRate;
    Glottis glottis;
    Tract tract;
    WhiteNoise whiteNoise;
    Biquad aspirateFilter;
    Biquad fricativeFilter;
    float pitch = rack::dsp::FREQ_C4;
    float fm = 0.f;
    float tongueX = 0.f, tongueY = 0.f;
    float constrictionX = 0.f, constrictionY = 0.f;
    float fricativeLevel = .5f, vibAmount = 0.f, tenseness = .5f;
    float newFricFC = 500.f, newQ = .5f;
    std::array<float, 15> spreadOffsets{};
    bool alwaysVoice = true;
    bool touchActive = false;

    PinkTromboneVoice(float rate, uint32_t seed)
        : sampleRate(rate), glottis(rate), tract(rate, 1.f / rate, &properties),
          whiteNoise(96000, seed), aspirateFilter(rate), fricativeFilter(rate) {
        initializeSimplexNoise();
        aspirateFilter.setFrequency(500.f);
        fricativeFilter.setFrequency(1000.f);
    }

    void setSampleRate(float rate) {
        sampleRate = rate;
        glottis.setSampleRate(rate);
        tract.setSampleRate(rate);
        aspirateFilter.setSampleRate(rate);
        fricativeFilter.setSampleRate(rate);
        // Settings are recalculated on the next control frame. Keep filters
        // below Nyquist immediately when switching to a lower sample rate.
        newFricFC = rack::clamp(newFricFC, 0.f, rate / 2.01f);
        aspirateFilter.setFrequency(newFricFC);
        fricativeFilter.setFrequency(newFricFC);
    }

    void updateControls(std::vector<rack::engine::Param>& params,
                        std::vector<rack::engine::Input>& inputs, int channel, bool polyphonic = false, const pinkSpeech::Pose* speech = nullptr) {
        auto voltage = [&](int id) {
            float value = inputs[id].getPolyVoltage(channel);
            return std::isfinite(value) ? value : 0.f;
        };
        auto value = [&](int id) {
            float knob = params[id].getValue();
            if (speech) {
                int control = pinkSpeech::controlForParam(id);
                if (control >= 0) return speech->controls[control] + knob - pinkSpeech::neutralKnobs[control];
            }
            return knob;
        };
        tract.setMovementSpeed(speech ? 100.f : 15.f);
        auto spread = [&](int row, float range) {
            return polyphonic ? spreadOffsets[row] * value(SPREAD_PARAM + row) * range : 0.f;
        };
        alwaysVoice = value(ALWAYS_VOICE_PARAM) >= .5f;
        fricativeLevel = rack::clamp(value(FRICL_PARAM) + value(FRICLA_PARAM) * voltage(FRICL_INPUT) / 10.f + spread(6, .25f), 0.f, 1.f);
        vibAmount = rack::clamp(value(VIB_PARAM) + value(VIBA_PARAM) * voltage(VIBI_INPUT) + spread(10, .25f), 0.f, 1.f);
        if (value(PITCH_WOBBLE_PARAM) < .5f) vibAmount = 0.f;
        // Match the knob's exponential Hz display and Rack's 1 V/oct standard.
        float octaves = rack::clamp(value(MASTERPITCH_PARAM) + value(VOCTA_PARAM) * voltage(VOCT_INPUT) + spread(8, 1.f / 12.f), -16.f, 16.f);
        pitch = rack::dsp::FREQ_C4 * std::pow(2.f, octaves);
        fm = rack::clamp(value(FMA_PARAM) + spread(9, .25f), -1.f, 1.f) * voltage(FMI_INPUT) * rack::dsp::FREQ_C4;
        glottis.setTargetFrequency(rack::clamp(pitch + fm, 2.f, std::min(sampleRate * .45f, rack::dsp::FREQ_C4 * 32.f)));
        tenseness = rack::clamp(value(TENSE_PARAM) + value(TENSEA_PARAM) * voltage(TENSEI_INPUT) / 10.f + spread(7, .25f), 0.f, 1.f);
        glottis.setTargetTenseness(tenseness);
        newFricFC = rack::clamp((value(FRICFC_PARAM) + spread(4, .25f)) * 1000.f + value(FRICFCA_PARAM) * voltage(FRICFCI_INPUT) * 2000.f + pitch * rack::clamp(value(FCFOLLOW_PARAM) + value(FCFOLLOWA_PARAM) * voltage(FCFOLLOW_INPUT) / 10.f + spread(14, .25f), 0.f, 1.f), 0.f, sampleRate / 2.01f);
        newQ = rack::clamp(value(FRICQ_PARAM) + value(FRICQA_PARAM) * voltage(FRICQI_INPUT) + spread(5, .3f), .00001f, 1.5f);
        aspirateFilter.setFrequency(newFricFC);
        aspirateFilter.setQ(newQ);
        fricativeFilter.setFrequency(newFricFC);
        fricativeFilter.setQ(newQ);
        tongueX = rack::clamp(value(TONGUEX_PARAM) + value(TONGUEXA_PARAM) * voltage(TONGUEXI_INPUT) / 20.f + spread(0, .25f), 0.f, 1.f);
        tongueY = rack::clamp(value(TONGUEY_PARAM) + value(TONGUEYA_PARAM) * voltage(TONGUEYI_INPUT) / 20.f + spread(1, .25f), 0.f, 1.f);
        constrictionX = rack::clamp(value(THROATX_PARAM) + value(THROATXA_PARAM) * voltage(THROATXI_INPUT) / 20.f + spread(2, .25f), 0.f, 1.f);
        constrictionY = rack::clamp(value(THROATY_PARAM) + value(THROATYA_PARAM) * voltage(THROATYI_INPUT) / 20.f + spread(3, .25f), 0.f, 1.f);
        float noseControl = rack::clamp(value(NOSE_PARAM) + value(NOSEA_PARAM) * voltage(NOSE_INPUT) * .9f + spread(11, 2.25f), 1.f, 10.f);
        float bladeControl = rack::clamp(value(BLADE_PARAM) + value(BLADEA_PARAM) * voltage(BLADE_INPUT) / 10.f + spread(13, .25f), 0.f, 1.f);
        float tipControl = rack::clamp(value(TIP_PARAM) + value(TIPA_PARAM) * voltage(TIP_INPUT) * .9f + spread(12, 2.25f), 0.f, 9.f);
        int noseStart = rack::clamp(13 + int(noseControl), 1, properties.n - 2);
        if (noseStart != properties.noseStart) tract.invalidateReflections();
        properties.noseStart = noseStart;
        properties.bladeStart = int(1.f + bladeControl * 10.f);
        // The old mapping always put the tip behind the blade, sometimes
        // dividing by zero and reversing the tongue-control bounds.
        properties.tipStart = properties.lipStart - 1 - int(tipControl * 2.f);
        float tongueIndex = tongueX * (tract.tongueIndexUpperBound() - tract.tongueIndexLowerBound()) + tract.tongueIndexLowerBound();
        float tongueDiameter = tongueY * (3.5f - 2.05f) + 2.05f;
        tract.setRestDiameter(tongueIndex, tongueDiameter);
        tract.setConstriction(constrictionX * properties.n, constrictionY * 4.f - 2.f, 1.f);
    }

    float process(bool externalVoice = false, float voiceVoltage = 0.f,
                  bool externalNoise = false, float noiseVoltage = 0.f, const pinkSpeech::Pose* speech = nullptr) {
        glottis.setIntensity(speech || alwaysVoice || touchActive ? fricativeLevel : 0.f);
        glottis.setLoudness(speech ? speech->voicing : 1.f);
        // Rack audio is nominally +/-5 V; the physical model uses +/-1.
        if (!std::isfinite(voiceVoltage)) voiceVoltage = 0.f;
        if (!std::isfinite(noiseVoltage)) noiseVoltage = 0.f;
        float noise = externalNoise ? noiseVoltage * .2f : externalVoice ? 0.f : whiteNoise.runStep();
        if (speech) noise *= speech->noise;
        float aspiration = aspirateFilter.runStep(noise);
        float frication = fricativeFilter.runStep(noise);
        float glottal = externalVoice ? voiceVoltage * .2f : glottis.runStep(0.f, aspiration);
        // An external noise path must not be gated by the external audio's
        // instantaneous amplitude (including silence and zero crossings).
        float noiseModulator = externalVoice || speech ? 1.f : glottal;
        tract.runStep(glottal, frication, 0.f, noiseModulator);
        double output = tract.lipOutput + tract.noseOutput;
        tract.runStep(glottal, frication, 0.f, noiseModulator);
        output += tract.lipOutput + tract.noseOutput;
        glottis.finishBlock(vibAmount);
        tract.finishBlock();
        if (!std::isfinite(output)) {
            tract.resetWaves();
            aspirateFilter.reset();
            fricativeFilter.reset();
            return 0.f;
        }
        return rack::clamp(float(output) * (speech ? speech->gain : 1.f), -10.f, 10.f);
    }

    void updateLights(std::vector<rack::engine::Light>& lights) {
        lights[L_LIGHT].setBrightness(tongueX);
        lights[L_LIGHT + 2].setBrightness(tongueY);
        lights[L_LIGHT + 1].setBrightness(tongueX * tongueY);
        lights[MAIN_LIGHT].setBrightness(pitch / (sampleRate / 16.f));
        lights[MAIN_LIGHT + 1].setBrightness(fm);
        lights[MAIN_LIGHT + 2].setBrightness(vibAmount);
        lights[R_LIGHT].setBrightness(constrictionX);
        lights[R_LIGHT + 2].setBrightness(constrictionY * 20.f - 11.5f);
        lights[R_LIGHT + 1].setBrightness(constrictionX * constrictionY * 20.f - 11.5f);
        lights[D_LIGHT].setBrightness(tenseness);
        lights[D_LIGHT + 1].setBrightness(tract.lipOutput);
        lights[D_LIGHT + 2].setBrightness(tract.noseOutput);
        lights[U_LIGHT].setBrightness(newFricFC / (sampleRate / 20.f));
        lights[U_LIGHT + 1].setBrightness(1.2f - newQ);
        lights[U_LIGHT + 2].setBrightness(fricativeLevel);
    }
};
