#include "../src/PinkTrombone.cpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <limits>

Plugin* pluginInstance = nullptr;
static size_t comparisons = 0;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void exact(float actual, float expected, const char* message) {
    ++comparisons;
    require(std::memcmp(&actual, &expected, sizeof(float)) == 0, message);
}
void valid(float sample) {
    require(std::isfinite(sample) && sample >= -10.f && sample <= 10.f, "non-finite or unsafe output");
}
struct TractTestAccess {
    static int transients(Tract& t) { return t.transientCount; }
    static void add(Tract& t, int position) { t.addTransient(position); }
    static void stepTransients(Tract& t) { t.processTransients(); }
    static void turbulence(Tract& t, float position) { t.addTurbulenceNoiseAtIndex(1.f, position, .5f, 1.f); }
    static void setNoseWave(Tract& t) { t.noseR[0] = .5f; t.noseReflection[1] = .5f; }
    static float noseWave(Tract& t) { return t.noseR[1]; }
    static float rest(Tract& t, int i) { return t.restDiameter[i]; }
    static float noseReflection(Tract& t) { return t.noseReflection[1]; }
    static float expectedNoseReflection(Tract& t) {
        float first = t.noseDiameter[0] * t.noseDiameter[0];
        float second = t.noseDiameter[1] * t.noseDiameter[1];
        return (first - second) / (first + second);
    }
    static bool finite(Tract& t) {
        for (int i = 0; i < t.tractProps->n; ++i)
            if (!std::isfinite(t.diameter[i]) || !std::isfinite(t.restDiameter[i])) return false;
        return true;
    }
};
void bugRegressions() {
    PinkTromboneVoice voice(1000.f, 1);
    auto& tract = voice.tract;
    for (int i = 0; i < MAX_TRANSIENTS; ++i) TractTestAccess::add(tract, 5);
    require(TractTestAccess::transients(tract) == MAX_TRANSIENTS, "transient pool initialization");
    for (int i = 0; i < 500; ++i) TractTestAccess::stepTransients(tract);
    require(TractTestAccess::transients(tract) == 0, "expired transients must leave the pool");
    TractTestAccess::add(tract, 5);
    require(TractTestAccess::transients(tract) == 1, "transient pool must be reusable");
    for (float position : {-100.f, 0.f, 41.99f, 42.f, 43.f, 44.f, 100.f, NAN, INFINITY})
        TractTestAccess::turbulence(tract, position);
    tract.resetWaves();
    TractTestAccess::setNoseWave(tract);
    tract.runStep(0.f, 0.f, 0.f, 0.f);
    require(std::fabs(TractTestAccess::noseWave(tract) - .25f) < 1e-7f, "nasal scattering must retain fractional waves");
    voice.properties.bladeStart = voice.properties.tipStart = 5;
    tract.setRestDiameter(10.f, 3.f);
    tract.finishBlock();
    require(TractTestAccess::finite(tract), "equal tip/blade must not divide by zero");

    Biquad filter(48000.f), uncached(48000.f);
    for (int i = 0; i < 5000; ++i) {
        float frequency = 100.f + (i / 300) * 50.f;
        filter.setFrequency(frequency); filter.setQ(.7f);
        uncached.setFrequency(frequency + 1.f); uncached.setFrequency(frequency);
        uncached.setQ(.8f); uncached.setQ(.7f);
        float signal = std::sin(i * .07f);
        exact(filter.runStep(signal), uncached.runStep(signal), "coefficient cache must preserve samples");
    }
    filter.setFrequency(0.f);
    for (int i = 0; i < 1000; ++i) exact(filter.runStep(1.f), 0.f, "zero cutoff must be silent and stable");
    WhiteNoise a(200, 123), b(200, 123), other(200, 124);
    bool independent = false;
    for (int i = 0; i < 1000; ++i) {
        float noise = a.runStep();
        exact(noise, b.runStep(), "noise seeds must be reproducible");
        independent |= noise != other.runStep();
    }
    require(independent, "voice noise streams must be independent");
    std::cout << "Bug regressions passed\n";
}
float cv(int frame, int input, int channel) {
    return float((frame / 47 * (channel + 1) + 13 * input + channel * 3) % 101 - 50) / 10.f;
}
void routing() {
    PinkTrombone module(123);
    module.outputs[0].channels = 1;
    std::array<std::unique_ptr<PinkTromboneVoice>, 16> references;
    for (int c = 0; c < 16; ++c) {
        references[c].reset(new PinkTromboneVoice(48000.f, 123 + 0x9e3779b9u * c));
        references[c]->spreadOffsets = module.voices[c]->spreadOffsets;
    }
    std::vector<rack::engine::Input> inputs(PinkTrombone::INPUTS_LEN);
    int previous = 0;
    const int counts[] = {1, 2, 3, 4, 8, 16, 2, 1, 16};
    for (int frame = 0; frame < 18000; ++frame) {
        int channels = counts[frame / 2000];
        float rate = frame < 6000 ? 44100.f : frame < 12000 ? 96000.f : 48000.f;
        for (size_t p = 0; p < module.params.size(); ++p)
            if (frame % 1800 == 0) {
                auto* q = module.paramQuantities[p];
                float value = q->minValue + (q->maxValue - q->minValue) * ((p + frame / 1800) % 7) / 6.f;
                module.params[p].setValue(value);
            }
        for (size_t i = 0; i < module.inputs.size(); ++i) {
            int width = i == PinkTrombone::VOCT_INPUT ? channels : i % 3 == 0 ? 1 : i % 3 == 1 ? std::min(2, channels) : 0;
            module.inputs[i].channels = width;
            for (int c = 0; c < 16; ++c) module.inputs[i].setVoltage(c < width ? cv(frame, i, c) : 0.f, c);
        }
        float expected[16] = {};
        for (int c = 0; c < channels; ++c) {
            auto& ref = *references[c];
            for (size_t i = 0; i < inputs.size(); ++i) {
                inputs[i].channels = module.inputs[i].isConnected() ? 1 : 0;
                inputs[i].setVoltage(module.inputs[i].getPolyVoltage(c));
            }
            if (ref.sampleRate != rate) ref.setSampleRate(rate);
            if (channels != previous) ref.updateControls(module.params, inputs, 0, channels > 1);
            float sample = ref.process(inputs[PinkTrombone::EXCITATION_INPUT].isConnected(), inputs[PinkTrombone::EXCITATION_INPUT].getVoltage(),
                inputs[PinkTrombone::NOISE_INPUT].isConnected(), inputs[PinkTrombone::NOISE_INPUT].getVoltage());
            expected[c] = sample;
            if ((frame + 1) % 24 == 0) ref.updateControls(module.params, inputs, 0, channels > 1);
        }
        module.process({rate, 1.f / rate, frame});
        require(module.outputs[0].getChannels() == channels, "wrong polyphonic channel count");
        for (int c = 0; c < channels; ++c) {
            valid(module.outputs[0].getVoltage(c));
            exact(module.outputs[0].getVoltage(c), expected[c], "polyphonic channel differs from independent mono voice");
        }
        previous = channels;
    }
    std::cout << "Mono/poly routing, broadcast, channel changes and rate changes passed\n";
}
void waveguideSimd() {
    PinkTrombone controls(15);
    float maximumError = 0.f;
    double energy = 0;
    for (float rate : {22050.f, 44100.f, 48000.f, 96000.f, 192000.f}) {
        PinkTromboneVoice scalar(rate, 789), vector(rate, 789);
        scalar.tract.useSimd = false;
        for (int frame = 0; frame < 12000; ++frame) {
            if (frame % 24 == 0) {
                controls.params[PinkTrombone::TONGUEX_PARAM].setValue((frame / 2400) * .1f);
                controls.params[PinkTrombone::NOSE_PARAM].setValue(1 + (frame / 2400) * 2);
                scalar.updateControls(controls.params, controls.inputs, 0);
                vector.updateControls(controls.params, controls.inputs, 0);
            }
            float a = scalar.process(), b = vector.process();
            valid(a); valid(b);
            maximumError = std::max(maximumError, std::fabs(a - b));
            require(std::fabs(a - b) <= .0002f, "SIMD waveguide differs excessively from scalar");
            energy += double(b) * b;
        }
    }
    require(energy > .001, "default voice must generate audio");
    std::cout << "SIMD/scalar passed; max error " << maximumError << " V\n";
}
void stress() {
    PinkTrombone module(453);
    module.outputs[0].channels = 1;
    for (float rate : {22050.f, 44100.f, 48000.f, 96000.f, 192000.f}) {
        for (int frame = 0; frame < 2500; ++frame) {
            if (frame % 100 == 0)
                for (size_t p = 0; p < module.params.size(); ++p) {
                    auto* q = module.paramQuantities[p];
                    float fraction = ((frame / 100 + p) % 5) * .25f;
                    module.params[p].setValue(q->minValue + (q->maxValue - q->minValue) * fraction);
                }
            for (size_t i = 0; i < module.inputs.size(); ++i) {
                module.inputs[i].channels = 16;
                for (int c = 0; c < 16; ++c) {
                    float value = cv(frame, i, c) * 20.f;
                    if ((frame + c + i) % 103 == 0) value = NAN;
                    if ((frame + c + i) % 211 == 0) value = INFINITY;
                    module.inputs[i].setVoltage(value, c);
                }
            }
            module.process({rate, 1.f / rate, frame});
            for (int c = 0; c < 16; ++c) valid(module.outputs[0].getVoltage(c));
        }
    }
    module.params[PinkTrombone::MASTERPITCH_PARAM].setValue(1.f);
    module.params[PinkTrombone::VOCTA_PARAM].setValue(1.f);
    module.inputs[PinkTrombone::VOCT_INPUT].setVoltage(1.f);
    module.voices[0]->updateControls(module.params, module.inputs, 0);
    require(std::fabs(module.voices[0]->pitch - 4 * rack::dsp::FREQ_C4) < .001f, "master pitch and 1 V/oct must agree with knob display");
    std::cout << "Extreme controls, malformed CV, pitch scaling and sample rates passed\n";
}
void independence() {
    PinkTrombone changed(1234), original(1234);
    changed.outputs[0].channels = original.outputs[0].channels = 1;
    for (auto* module : {&changed, &original}) {
        module->inputs[PinkTrombone::VOCT_INPUT].channels = 16;
        module->params[PinkTrombone::TONGUEXA_PARAM].setValue(1.f);
        module->inputs[PinkTrombone::TONGUEXI_INPUT].channels = 16;
    }
    bool diverged = false;
    for (int frame = 0; frame < 8000; ++frame) {
        if (frame == 2000) changed.inputs[PinkTrombone::TONGUEXI_INPUT].setVoltage(10.f, 5);
        rack::engine::Module::ProcessArgs args{48000.f, 1.f / 48000.f, frame};
        changed.process(args); original.process(args);
        for (int c = 0; c < 16; ++c)
            if (c != 5) exact(changed.outputs[0].getVoltage(c), original.outputs[0].getVoltage(c), "one voice's CV must not affect another voice");
            else diverged |= changed.outputs[0].getVoltage(c) != original.outputs[0].getVoltage(c);
    }
    require(diverged, "targeted voice must respond to its CV");
    std::cout << "Voice isolation passed\n";
}
void threadedNoise() {
    std::array<std::thread, 8> threads;
    std::array<float, 8> values;
    for (int i = 0; i < 8; ++i) threads[i] = std::thread([&, i] {
        values[i] = 0.f;
        for (int j = 0; j < 1000; ++j) values[i] += simplex1(j * .031f);
    });
    for (auto& thread : threads) thread.join();
    for (int i = 1; i < 8; ++i) exact(values[i], values[0], "concurrent simplex lookup must agree");
    std::cout << "Concurrent simplex initialization passed\n";
}
void geometryAndControls() {
    for (float index = 0; index <= 45; index += .5f)
        for (float diameter = -1.7f; diameter <= 3.5f; diameter += .2f) {
            auto point = pinkGeometry::point(index, diameter);
            require(std::fabs(pinkGeometry::index(point) - index) < .00002f, "tract hit testing must invert the original drawing");
            require(std::fabs(pinkGeometry::diameter(point) - diameter) < .00001f, "tract diameter hit testing must invert the drawing");
        }
    bool params[PinkTrombone::PARAMS_LEN] = {}, inputs[PinkTrombone::INPUTS_LEN] = {};
    for (auto& row : pinkUI::rows) {
        if (row.value >= 0) params[row.value] = true;
        params[row.amount] = true; inputs[row.input] = true;
    }
    for (auto& row : pinkUI::extras) {
        params[row.value] = true; params[row.amount] = true; inputs[row.input] = true;
    }
    params[PinkTrombone::ALWAYS_VOICE_PARAM] = params[PinkTrombone::PITCH_WOBBLE_PARAM] = true;
    for (int row = 0; row < 15; ++row) params[PinkTrombone::SPREAD_PARAM + row] = true;
    inputs[PinkTrombone::EXCITATION_INPUT] = inputs[PinkTrombone::NOISE_INPUT] = true;
    for (bool exposed : params) require(exposed, "every parameter must remain exposed in the panel");
    for (bool exposed : inputs) require(exposed, "every CV input must remain exposed in the panel");
    PinkTrombone module(12);
    for (auto* quantity : module.paramQuantities)
        require(quantity->minValue <= quantity->maxValue, "knobs must have ordered bounds");
    module.params[PinkTrombone::NOSEA_PARAM].setValue(1);
    module.inputs[PinkTrombone::NOSE_INPUT].channels = 1;
    module.inputs[PinkTrombone::NOSE_INPUT].setVoltage(10.f);
    module.voices[0]->updateControls(module.params, module.inputs, 0);
    require(module.voices[0]->properties.noseStart == 23, "new nose CV must affect geometry");
    require(module.voices[0]->tract.tongueIndexUpperBound() > module.voices[0]->tract.tongueIndexLowerBound(), "tongue UI bounds must be correctly ordered");
    std::cout << "Anatomical hit testing, complete panel controls and geometry CV passed\n";
}

void switchesAndSpread() {
    PinkTrombone module(56);
    module.params[PinkTrombone::FRICL_PARAM].setValue(.61f);
    module.params[PinkTrombone::VIB_PARAM].setValue(.23f);
    module.params[PinkTrombone::ALWAYS_VOICE_PARAM].setValue(0.f);
    module.params[PinkTrombone::PITCH_WOBBLE_PARAM].setValue(0.f);
    auto& voice = *module.voices[0];
    voice.updateControls(module.params, module.inputs, 0);
    require(!voice.alwaysVoice && voice.vibAmount == 0.f, "switches must control their own DSP state");
    exact(module.params[PinkTrombone::FRICL_PARAM].getValue(), .61f, "voice switch must preserve level knob");
    exact(module.params[PinkTrombone::VIB_PARAM].getValue(), .23f, "wobble switch must preserve vibrato knob");
    for (int frame = 0; frame < 1000; ++frame) exact(voice.process(), 0.f, "unvoiced idle voice must be silent");
    voice.touchActive = true;
    bool sounding = false;
    for (int frame = 0; frame < 1000; ++frame) sounding |= voice.process() != 0.f;
    require(sounding, "touching the voicebox must sound when always voice is off");
    module.params[PinkTrombone::ALWAYS_VOICE_PARAM].setValue(1.f);
    module.params[PinkTrombone::PITCH_WOBBLE_PARAM].setValue(1.f);
    voice.updateControls(module.params, module.inputs, 0);
    require(voice.alwaysVoice && voice.vibAmount == .23f, "switches must restore retained settings");
    module.params[PinkTrombone::SPREAD_PARAM].setValue(1.f);
    module.params[PinkTrombone::TONGUEX_PARAM].setValue(.5f);
    for (auto& v : module.voices) v->updateControls(module.params, module.inputs, 0, true);
    bool varied = false;
    for (int c = 1; c < 16; ++c) varied |= module.voices[c]->tongueX != voice.tongueX;
    require(varied, "spread must vary voices independently");
    float offset = voice.tongueX;
    voice.updateControls(module.params, module.inputs, 0, true);
    exact(voice.tongueX, offset, "spread must not jitter between updates");
    json_t* saved = module.dataToJson();
    PinkTrombone restored(999);
    restored.dataFromJson(saved); json_decref(saved);
    for (int c = 0; c < 16; ++c)
        for (int row = 0; row < 15; ++row)
            exact(restored.voices[c]->spreadOffsets[row], module.voices[c]->spreadOffsets[row], "voice personalities must survive saving");
    voice.updateControls(module.params, module.inputs, 0, false);
    exact(voice.tongueX, .5f, "mono operation must ignore spread");
    module.display.channels.store(1);
    require(!pinkUI::polyphonic(&module), "spread column must be hidden in mono");
    module.display.channels.store(16);
    require(pinkUI::polyphonic(&module), "spread column must appear in polyphony");
    std::cout << "Independent switch states, touch voicing and stable polyphonic spread passed\n";
}
void externalAudio() {
    PinkTromboneVoice a(48000.f, 1), b(48000.f, 999);
    bool impulse = false, tail = false;
    for (int frame = 0; frame < 6000; ++frame) {
        float input = frame == 100 ? 5.f : 0.f;
        float sample = a.process(true, input);
        exact(sample, b.process(true, input), "external voice must not include random internal noise");
        valid(sample);
        if (frame < 100) exact(sample, 0.f, "silent external input must suppress the internal source");
        if (frame > 100) impulse |= sample != 0.f;
        if (frame > 200) tail |= sample != 0.f;
    }
    require(impulse && tail, "external impulse must excite the physical waveguide");
    PinkTrombone module(8);
    module.outputs[0].channels = 1;
    module.inputs[PinkTrombone::EXCITATION_INPUT].channels = 16;
    for (int frame = 0; frame < 1500; ++frame) {
        for (int c = 0; c < 16; ++c) module.inputs[PinkTrombone::EXCITATION_INPUT].setVoltage(c == 7 && frame == 0 ? 5.f : 0.f, c);
        module.process({48000.f, 1.f / 48000.f, frame});
        require(module.outputs[0].getChannels() == 16, "external audio must determine polyphony");
        for (int c = 0; c < 16; ++c)
            if (c != 7) exact(module.outputs[0].getVoltage(c), 0.f, "external audio channels must remain isolated");
    }
    PinkTromboneVoice noiseA(48000.f, 1), noiseB(48000.f, 999);
    for (int frame = 0; frame < 1000; ++frame) {
        float noise = std::sin(frame * .31f) * 5.f;
        exact(noiseA.process(false, 0.f, true, noise), noiseB.process(false, 0.f, true, noise), "noise jack must replace the random source");
    }
    module.inputs[PinkTrombone::EXCITATION_INPUT].channels = 3;
    for (int c = 0; c < 3; ++c) module.inputs[PinkTrombone::EXCITATION_INPUT].setVoltage((c - 1) * 2.f, c);
    module.processBypass({48000.f, 1.f / 48000.f, 0});
    require(module.outputs[0].getChannels() == 3, "effect bypass must preserve input polyphony");
    for (int c = 0; c < 3; ++c)
        exact(module.outputs[0].getVoltage(c), (c - 1) * 2.f, "effect bypass must pass dry audio unchanged");
    std::cout << "External excitation/noise replacement, silence, impulse, channel isolation and dry bypass passed\n";
}

struct GlottisTestAccess {
    static void setTime(Glottis& g, double time) { g.totalTime = time; }
    static double time(Glottis& g) { return g.totalTime; }
};
void reviewRegressions() {
    // Float clocks stop advancing at these magnitudes at high sample rates.
    for (auto setting : {std::pair<float, double>{96000.f, 256.}, {192000.f, 128.}}) {
        Glottis glottis(setting.first);
        GlottisTestAccess::setTime(glottis, setting.second);
        glottis.runStep(0.f, 0.f);
        require(GlottisTestAccess::time(glottis) > setting.second, "long-running vibrato clock must keep advancing");
    }
    PinkTromboneVoice moved(48000.f, 7), fresh(48000.f, 7);
    moved.properties.bladeStart = 1;
    moved.tract.setRestDiameter(20.f, 2.7f);
    moved.properties.bladeStart = fresh.properties.bladeStart = 11;
    moved.tract.setRestDiameter(20.f, 2.7f);
    fresh.tract.setRestDiameter(20.f, 2.7f);
    for (int i = 0; i < 44; ++i)
        exact(TractTestAccess::rest(moved.tract, i), TractTestAccess::rest(fresh.tract, i), "tract target must not depend on prior blade positions");
    PinkTromboneVoice nasal(1000.f, 7);
    exact(TractTestAccess::noseReflection(nasal.tract), TractTestAccess::expectedNoseReflection(nasal.tract), "initial nasal reflection must match the velum");
    float closed = TractTestAccess::noseReflection(nasal.tract);
    nasal.tract.setConstriction(20.f, -1.f, 1.f);
    for (int frame = 0; frame < 150; ++frame) {
        nasal.tract.finishBlock();
        exact(TractTestAccess::noseReflection(nasal.tract), TractTestAccess::expectedNoseReflection(nasal.tract), "nasal reflection must follow the moving velum");
    }
    require(TractTestAccess::noseReflection(nasal.tract) != closed, "opening the velum must change nasal coupling");
    PinkTromboneVoice effect(48000.f, 7);
    effect.tract.setConstriction(10.f, .5f, 1.f);
    bool sounding = false;
    for (int frame = 0; frame < 1500; ++frame) {
        float sample = effect.process(true, 0.f, true, 5.f * std::sin(frame * .3f));
        valid(sample); sounding |= sample != 0.f;
    }
    require(sounding, "external noise must excite a constriction with a silent voice input");
    std::cout << "Review regressions: long-running clocks, blade history, nasal coupling and independent external noise passed\n";
}

volatile float benchmarkSink = 0;
double benchmark(bool useSimd) {
    PinkTrombone module(42);
    module.outputs[0].channels = 1;
    module.inputs[PinkTrombone::VOCT_INPUT].channels = 16;
    for (auto& voice : module.voices) voice->tract.useSimd = useSimd;
    auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 30000; ++frame) {
        module.process({48000.f, 1.f / 48000.f, frame});
        benchmarkSink = module.outputs[0].getVoltage();
    }
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}
int main(int argc, char** argv) {
    // The standalone harness owns the model factory normally owned by Rack.
    std::unique_ptr<Model> factoryOwner(modelPinkTrombone);
    try {
        if (argc == 2 && std::string(argv[1]) == "--benchmark") {
            double scalar = 1e9, vector = 1e9;
            for (int trial = 0; trial < 3; ++trial) {
                scalar = std::min(scalar, benchmark(false));
                vector = std::min(vector, benchmark(true));
            }
            std::cout << "16 voices: " << scalar / vector << "x SIMD speedup; "
                      << vector / 30000 * 1e6 << " us/sample vs " << scalar / 30000 * 1e6 << " scalar\n";
            return 0;
        }
        threadedNoise(); reviewRegressions(); bugRegressions(); routing(); waveguideSimd(); stress(); independence(); geometryAndControls(); switchesAndSpread(); externalAudio();
        std::cout << comparisons << " exact sample comparisons passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
