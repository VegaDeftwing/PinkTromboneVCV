// Included by the main regression harness so it exercises the actual modules.
#include <fstream>
#include <chrono>

void englishFrontend() {
    auto hello = pinkSpeech::compileEnglish("Hello world!");
    require(hello.duration > .5 && hello.duration < 5., "English sentence must have natural bounded timing");
    require(hello.pronunciation.find("hh ax l ow") != std::string::npos, "dictionary pronunciation of hello");
    require(hello.pronunciation.find("w er l d") != std::string::npos, "dictionary pronunciation of world");
    auto numbers = pinkSpeech::compileEnglish("42 dollars and 3 cats.");
    bool forty = false, two = false, three = false;
    for (const auto& segment : numbers.segments) {
        forty |= segment.word == "forty"; two |= segment.word == "two"; three |= segment.word == "three";
    }
    require(forty && two && three, "numbers must be normalized into words");
    auto unknown = pinkSpeech::compileEnglish("Flibberwomps snorfle quazatron.");
    require(unknown.segments.size() > 10, "unfamiliar words must have letter-to-sound pronunciations");
    auto typography = pinkSpeech::compileEnglish("Caf\xc3\xa9 isn't \xe2\x80\x9cordinary\xe2\x80\x9d!");
    require(!typography.segments.empty(), "common Unicode typography must normalize safely");
    require(pinkSpeech::compileEnglish("   \n ... \xf0\x9f\x98\x80").segments.empty(), "non-speech text must not produce garbage speech");
    bool rejected = false;
    try { pinkSpeech::compileEnglish(std::string(pinkSpeech::maxTextBytes + 1, 'a')); } catch (const std::exception&) { rejected = true; }
    require(rejected, "unbounded text must be rejected safely");
    for (const auto* plan : {&hello, &numbers, &unknown, &typography}) {
        size_t cursor = 0;
        for (double t = 0.; t < plan->duration; t += .001) {
            auto pose = plan->at(t, cursor);
            for (float value : pose.controls) require(std::isfinite(value), "speech controls must remain finite");
            require(pose.gain >= 0.f && pose.gain <= 1.f && pose.voicing >= 0.f && pose.voicing <= 1.f, "speech envelopes must remain bounded");
        }
    }
    std::cout << "English dictionary, LTS fallback, numbers, typography and timing passed\n";
}
void speechOffsets() {
    PinkTrombone module(121);
    auto& voice = *module.voices[0];
    pinkSpeech::Pose pose;
    pose.controls[0] = .4f; pose.controls[8] = 0.f;
    voice.updateControls(module.params, module.inputs, 0, false, &pose);
    require(std::fabs(voice.tongueX - .4f) < 1e-6f, "neutral knob must follow the speech target");
    module.params[PinkTrombone::TONGUEX_PARAM].setValue(.2f);
    voice.updateControls(module.params, module.inputs, 0, false, &pose);
    require(std::fabs(voice.tongueX - .6f) < 1e-6f, "base knob must become an offset");
    module.params[PinkTrombone::TONGUEXA_PARAM].setValue(1.f);
    module.inputs[PinkTrombone::TONGUEXI_INPUT].channels = 1;
    module.inputs[PinkTrombone::TONGUEXI_INPUT].setVoltage(-4.f);
    voice.updateControls(module.params, module.inputs, 0, false, &pose);
    require(std::fabs(voice.tongueX - .4f) < 1e-6f, "CV must remain additive during speech");
    voice.updateControls(module.params, module.inputs, 0);
    require(std::fabs(voice.tongueX) < 1e-6f, "manual operation must resume without rewriting knobs");
    exact(module.params[PinkTrombone::TONGUEX_PARAM].getValue(), .2f, "speech must not rewrite stored base parameters");
    std::cout << "Speech targets, base offsets, additive CV and manual restoration passed\n";
}
void waitForSpeech(PinkSpeech& speech) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        auto info = speech.compiler.info();
        if (!info.error.empty()) throw std::runtime_error(info.error);
        if (info.ready == info.requested && !info.loading) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("speech compiler timed out");
}
void flipSpeechMessage(PinkTrombone& host) {
    if (host.leftExpander.messageFlipRequested) {
        std::swap(host.leftExpander.producerMessage, host.leftExpander.consumerMessage);
        host.leftExpander.messageFlipRequested = false;
    }
}
void speechExpander() {
    PinkTrombone host(32);
    PinkSpeech speech;
    host.model = modelPinkTrombone; speech.model = modelPinkSpeech;
    host.leftExpander.module = &speech; speech.rightExpander.module = &host;
    connectOutputs(host); connectOutputs(speech);
    speech.compiler.request("Hello world.");
    waitForSpeech(speech);
    speech.params[PinkSpeech::VOICES_PARAM].setValue(2.f);
    float rate = 48000.f;
    int endCount = 0;
    bool previousEnd = false, gate = false;
    double energy = 0.;
    for (int frame = 0; frame < 180000; ++frame) {
        speech.params[PinkSpeech::PLAY_PARAM].setValue(frame == 24 ? 1.f : 0.f);
        Module::ProcessArgs args{rate, 1.f / rate, frame};
        speech.process(args); host.process(args); flipSpeechMessage(host);
        bool end = speech.outputs[PinkSpeech::END_OUTPUT].getVoltage() > 1.f;
        if (end && !previousEnd) ++endCount;
        previousEnd = end;
        gate |= speech.outputs[PinkSpeech::GATE_OUTPUT].getVoltage() > 1.f;
        float sample = host.outputs[0].getVoltage();
        valid(sample); energy += sample * sample;
        if (frame < 24) require(sample == 0.f, "attached idle expander must keep host silent");
    }
    require(host.outputs[0].getChannels() == 2, "expander must drive host polyphony");
    require(gate && endCount == 1 && energy > 1., "speech must produce audio, speaking gate and one end pulse");
    require(!speech.speaking.load() && speech.outputs[PinkSpeech::GATE_OUTPUT].getVoltage() == 0.f, "speech must finish and go idle");
    exact(host.params[PinkTrombone::MASTERPITCH_PARAM].getValue(), 0.f, "expander must not overwrite host knob state");
    host.leftExpander.module = nullptr;
    host.process({48000.f, 1.f / 48000.f, 180001});
    require(!host.display.speechActive.load(), "detaching must restore the manual mode");
    require(host.voices[0]->pitch == rack::dsp::FREQ_C4, "manual pitch must restore immediately");
    std::cout << "Expander transport, speech audio, idle silence, gate/end and detach passed\n";
}
void speechPlaybackState() {
    PinkTrombone host(22);
    PinkSpeech speech;
    host.model = modelPinkTrombone; speech.model = modelPinkSpeech;
    host.leftExpander.module = &speech; speech.rightExpander.module = &host;
    connectOutputs(speech);
    speech.params[PinkSpeech::VOICES_PARAM].setValue(2.f);
    speech.inputs[PinkSpeech::PLAY_INPUT].channels = 2;
    int frame = 0;
    auto tick = [&] { speech.process({48000.f, 1.f/48000.f, frame++}); flipSpeechMessage(host); };
    speech.compiler.request("An old phrase continues independently.");
    waitForSpeech(speech); tick();
    speech.inputs[PinkSpeech::PLAY_INPUT].setVoltage(10.f, 0); tick();
    speech.inputs[PinkSpeech::PLAY_INPUT].setVoltage(0.f, 0); tick();
    auto* original = speech.players[0].utterance;
    require(original && original->audioReferences == 2, "current utterance and playing voice must own their plans");
    speech.compiler.request("New words.");
    waitForSpeech(speech); tick();
    require(speech.plan != original && speech.players[0].utterance == original, "editing must not replace a playing voice's utterance");
    speech.inputs[PinkSpeech::PLAY_INPUT].setVoltage(10.f, 1); tick();
    require(speech.players[1].utterance == speech.plan && speech.players[0].utterance == original, "a poly trigger must only start its own voice using the new text");
    speech.inputs[PinkSpeech::PLAY_INPUT].setVoltage(0.f, 1); tick();
    speech.compiler.request("Queued speech after compilation.");
    speech.requestPlay(); tick();
    require(speech.players[0].queued && speech.players[1].queued, "Speak during compilation must queue the current text");
    speech.params[PinkSpeech::STOP_PARAM].setValue(1.f); tick();
    speech.params[PinkSpeech::STOP_PARAM].setValue(0.f); tick();
    waitForSpeech(speech); tick();
    require(!speech.players[0].playing && !speech.players[0].queued, "Stop must cancel a queued start");
    speech.compiler.request("Go."); speech.requestPlay(); tick();
    waitForSpeech(speech); tick();
    require(speech.players[0].playing && speech.players[1].playing, "queued Speak must start when the requested text is ready");
    speech.params[PinkSpeech::LOOP_PARAM].setValue(1.f);
    speech.params[PinkSpeech::RATE_PARAM].setValue(3.f);
    speech.inputs[PinkSpeech::PITCH_INPUT].channels = 1;
    speech.inputs[PinkSpeech::PITCH_INPUT].setVoltage(1.f);
    tick();
    size_t pitchCursor = 0;
    auto expected = speech.plan->at(speech.players[0].time, pitchCursor).controls[8] + 1.f;
    require(std::fabs(speech.frame.voices[0].controls[8] - expected) < .01f, "speech pitch CV must use 1 V/oct");
    int endings = 0; bool wasEnd = false;
    for (int i = 0; i < 48000; ++i) {
        tick(); bool end = speech.outputs[PinkSpeech::END_OUTPUT].getVoltage() > 0.f;
        endings += end && !wasEnd; wasEnd = end;
    }
    require(endings >= 2 && speech.players[0].playing, "Repeat must produce end pulses and restart playback");
    speech.processBypass({48000.f,1.f/48000.f,frame++}); flipSpeechMessage(host);
    require(!speech.players[0].playing && speech.frame.voices[0].gain == 0.f, "bypass must stop speech rather than sustain the last articulation");
    speech.requestPlay(); tick();
    speech.rightExpander.module = nullptr; tick();
    require(!speech.players[0].playing && !speech.players[1].playing, "disconnect must stop every player");
    speech.rightExpander.module = &host;
    speech.params[PinkSpeech::VOICES_PARAM].setValue(1.f);
    speech.inputs[PinkSpeech::PLAY_INPUT].channels = 0;
    speech.requestPlay(); tick();
    require(!speech.players[1].playing && speech.outputs[PinkSpeech::GATE_OUTPUT].getChannels() == 1, "shrinking channel count must retire inactive players");
    json_t* saved = speech.dataToJson();
    PinkSpeech restored; restored.dataFromJson(saved); json_decref(saved);
    require(restored.compiler.info().text == "Go.", "ordinary text must survive patch serialization");
    speech.resetSpeech();
    tick();
    require(!speech.players[0].playing && speech.compiler.info().text == "Hello. This is Pink Trombone.", "Initialize must reset text and stop playback");
    std::cout << "Speech plan lifetime, independent phrases, queued starts, stop, loop, pitch CV, bypass and persistence passed\n";
}
void speechWordsAndSmoothing() {
    auto repeated = pinkSpeech::compileEnglish("Go go go. 42!");
    require(repeated.words.size() == 5, "repeated words must remain separate; numbers must expand to spoken words");
    for (const auto& word : repeated.words) {
        require(word.end > word.start && word.firstSegment < repeated.segments.size(), "word ranges must be nonempty and address their first phone");
        require(repeated.segments[word.firstSegment].word == word.text, "word ranges must refer to their actual word");
    }
    PinkSpeech::Player sharp, soft, fast;
    pinkSpeech::Pose a, b; a.controls[0] = 0.f; b.controls[0] = 1.f; b.controls[8] = 2.f; b.gain = .5f; b.voicing = 1.f;
    PinkSpeech::smooth(sharp,a,0.f,.001f); PinkSpeech::smooth(soft,a,1.f,.001f); PinkSpeech::smooth(fast,a,1.f,.0005f);
    auto direct = PinkSpeech::smooth(sharp,b,0.f,.001f);
    exact(direct.controls[0], b.controls[0], "zero smoothing must preserve original targets exactly");
    auto filtered = PinkSpeech::smooth(soft,b,1.f,.001f);
    PinkSpeech::smooth(fast,b,1.f,.0005f); auto twice = PinkSpeech::smooth(fast,b,1.f,.0005f);
    require(filtered.controls[0] > 0.f && filtered.controls[0] < .01f, "smoothing must soften abrupt articulation changes");
    require(std::fabs(twice.controls[0] - filtered.controls[0]) < 1e-6f, "smoothing must have sample-rate-independent time constants");
    exact(filtered.controls[8], b.controls[8], "smoothing must not smear pitch");
    exact(filtered.gain, b.gain, "smoothing must preserve playback envelope timing");

    PinkTrombone host(3); PinkSpeech speech;
    host.model = modelPinkTrombone; speech.model = modelPinkSpeech;
    host.leftExpander.module = &speech; speech.rightExpander.module = &host;
    connectOutputs(speech);
    speech.compiler.request("One two three."); waitForSpeech(speech);
    int frame = 0, endings = 0; bool previousEnd = false;
    auto tick = [&] {
        speech.process({48000.f,1.f/48000.f,frame++}); flipSpeechMessage(host);
        bool end = speech.outputs[PinkSpeech::END_OUTPUT].getVoltage() > 0.f;
        endings += end && !previousEnd; previousEnd = end;
    };
    auto word = [&] { speech.params[PinkSpeech::WORD_PARAM].setValue(1.f); tick(); speech.params[PinkSpeech::WORD_PARAM].setValue(0.f); tick(); };
    auto finish = [&] { int bound = 240000; while (speech.players[0].playing && --bound) tick(); require(bound > 0, "word playback must terminate"); for(int i=0;i<100;++i) tick(); };
    tick(); require(speech.plan->words.size() == 3, "test phrase must have three words");
    speech.params[PinkSpeech::LOOP_PARAM].setValue(1.f);
    for (size_t expected : {size_t(0),size_t(1),size_t(2),size_t(0)}) {
        int before = endings; word();
        require(speech.players[0].wordMode && speech.players[0].wordIndex == expected, "word triggers must advance and wrap");
        require(speech.players[0].end == speech.plan->words[expected].end, "word trigger must stop at its own word boundary");
        finish(); require(endings == before + 1 && !speech.players[0].playing, "END must pulse once per word, with no automatic word repeat");
    }
    speech.inputs[PinkSpeech::WORD_INPUT].channels = 1;
    speech.inputs[PinkSpeech::WORD_INPUT].setVoltage(10.f); tick();
    require(speech.players[0].wordIndex == 1, "the Word CV jack must advance exactly like the button");
    size_t held = speech.players[0].wordIndex; tick();
    require(speech.players[0].wordIndex == held, "a held Word gate must not keep advancing");
    speech.inputs[PinkSpeech::WORD_INPUT].setVoltage(0.f); finish();
    speech.inputs[PinkSpeech::SELECT_INPUT].channels = 1; speech.inputs[PinkSpeech::SELECT_INPUT].setVoltage(10.f);
    word(); require(speech.players[0].wordIndex == 2, "10 V must select the last word"); finish();
    word(); require(speech.players[0].wordIndex == 2, "a patched selector must repeat the selected word"); finish();
    speech.inputs[PinkSpeech::SELECT_INPUT].setVoltage(-5.f); word();
    require(speech.players[0].wordIndex == 0, "negative selection must clamp to the first word"); finish();
    speech.inputs[PinkSpeech::SELECT_INPUT].channels = 0; word();
    require(speech.players[0].wordIndex == 2, "selection must leave the automatic sequencer position intact"); finish();
    speech.params[PinkSpeech::LOOP_PARAM].setValue(0.f);
    int before = endings; speech.requestPlay(); tick();
    require(!speech.players[0].wordMode && speech.players[0].end == speech.plan->duration, "Speak must still play the entire phrase");
    finish(); require(endings == before + 1, "phrase playback must emit one END at phrase completion, not at every word");
    speech.inputs[PinkSpeech::SELECT_INPUT].channels = 1; speech.inputs[PinkSpeech::SELECT_INPUT].setVoltage(10.f);
    speech.compiler.request("New words here."); word();
    require(speech.players[0].queued && speech.players[0].wordMode, "word triggers must queue during text compilation");
    speech.inputs[PinkSpeech::SELECT_INPUT].setVoltage(0.f); waitForSpeech(speech); tick();
    require(speech.players[0].playing && speech.players[0].wordIndex == 2, "queued word playback must retain trigger-time selection"); finish();
    speech.inputs[PinkSpeech::SELECT_INPUT].channels = 2;
    speech.inputs[PinkSpeech::SELECT_INPUT].setVoltage(0.f,0); speech.inputs[PinkSpeech::SELECT_INPUT].setVoltage(10.f,1);
    speech.inputs[PinkSpeech::SMOOTH_INPUT].channels = 2;
    speech.inputs[PinkSpeech::SMOOTH_INPUT].setVoltage(0.f,0); speech.inputs[PinkSpeech::SMOOTH_INPUT].setVoltage(10.f,1);
    word();
    require(speech.players[0].wordIndex == 0 && speech.players[1].wordIndex == 2, "polyphonic word selection must be independent per lane");
    for(int i=0;i<5000;++i) { tick(); for(const auto& pose:speech.frame.voices) for(float v:pose.controls) require(std::isfinite(v), "smoothed targets must remain finite"); }
    speech.compiler.request("... "); waitForSpeech(speech); word();
    require(!speech.players[0].playing && !speech.players[0].queued, "empty text must not leave a word player stuck");
    std::cout << "Word boundaries, stepping/wrap, selection, queued/poly playback, dynamic END and smoothing passed\n";
}
void renderSpeech(const char* path, const std::string& text) {
    auto plan = pinkSpeech::compileEnglish(text);
    PinkTrombone module(12);
    PinkTromboneVoice voice(48000.f, 42);
    size_t cursor = 0;
    std::vector<int16_t> samples;
    double peak = 0., energy = 0.;
    for (int frame = 0; frame < int((plan.duration + .1) * 48000); ++frame) {
        auto pose = plan.at(double(frame) / 48000, cursor);
        if (frame % 24 == 0) voice.updateControls(module.params, module.inputs, 0, false, &pose);
        float sample = voice.process(false, 0.f, false, 0.f, &pose);
        valid(sample); peak = std::max(peak, double(std::fabs(sample))); energy += double(sample)*sample;
        samples.push_back(int16_t(std::max(-32767.f, std::min(32767.f, sample * 6000.f))));
    }
    std::ofstream file(path, std::ios::binary);
    auto u16 = [&](uint16_t value) { char b[2] = {char(value), char(value >> 8)}; file.write(b,2); };
    auto u32 = [&](uint32_t value) { char b[4] = {char(value),char(value >> 8),char(value >> 16),char(value >> 24)}; file.write(b,4); };
    file.write("RIFF",4);u32(36+samples.size()*2);file.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(48000);u32(96000);u16(2);u16(16);file.write("data",4);u32(samples.size()*2);
    for (int16_t sample : samples) u16(uint16_t(sample));
    std::cout << plan.pronunciation << "\nRendered " << plan.duration << " seconds, peak " << peak << " V, RMS " << std::sqrt(energy/samples.size()) << " V\n";
}
