#include "plugin.hpp"
// Glottis and Tract are where all the magic happens. Neither is crazy complicated, though
// rewriting them to be per sample instead of working on blocks is going to be a challenge.
// Glottis doesn't appear to use any buffers, so it may be able to be used outright
// 
#include "PinkTrombone/Glottis.hpp"
#include "PinkTrombone/Tract.hpp"
// White noise is just filling a buffer with random values with rand(). This can
// be done per sample instead to bring us back to real time, probably using rack::random
#include "PinkTrombone/WhiteNoise.hpp"
// Biquad is a normal biquad filter, with frequency, Q, and gain. 
// dsp::Biquad from the VCV dsp library should be used instead of Pink Trombones
#include "PinkTrombone/Biquad.hpp"
// see https://vcvrack.com/docs-v2/structrack_1_1dsp_1_1TBiquadFilter
// dsp::BiquadFilter myFilter
// myFilter.setParameters(rack::dsp::BiquadFilter::HIGHPASS,normfreq,q,1.0f); // Type, Freq (normalized, must be less than .5), Q, Gain
// can be LOWPASS_1POLE, HIGHPASS_1POLE, LOWPASS, HIGHPASS, LOWSHELF, HIGHSHELF, BANDPASS, PEAK, or NOTCH 
#include "PinkTrombone/util.h"
// util has max, min, clamp, which are all in dsp:: however, "moveTowards" and "Gaussian" will both need adapted to whatever code is written

struct PinkTrombone : Module {
	enum ParamId {
		PITCHO_PARAM,
		PITCHA_PARAM,
		VOLO_PARAM,
		VOLA_PARAM,
		CAVITYXO_PARAM,
		CAVITYXA_PARAM,
		CAVITYYO_PARAM,
		CAVITYYA_PARAM,
		TONGUEXO_PARAM,
		TOUNGEXA_PARAM,
		TONGUEYO_PARAM,
		TOUNGEYA_PARAM,
		SOFTPALATE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		PITCH_INPUT,
		VOL_INPUT,
		CAVITYX_INPUT,
		CAVITYY_INPUT,
		TONGUEX_INPUT,
		TONGUEY_INPUT,
		SOFTPALATE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	PinkTrombone() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PITCHO_PARAM, 0.f, 1.f, 0.f, "");
		configParam(PITCHA_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLO_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLA_PARAM, 0.f, 1.f, 0.f, "");
		configParam(CAVITYXO_PARAM, 0.f, 1.f, 0.f, "");
		configParam(CAVITYXA_PARAM, 0.f, 1.f, 0.f, "");
		configParam(CAVITYYO_PARAM, 0.f, 1.f, 0.f, "");
		configParam(CAVITYYA_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TONGUEXO_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TOUNGEXA_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TONGUEYO_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TOUNGEYA_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SOFTPALATE_PARAM, 0.f, 1.f, 0.f, "");
		configInput(PITCH_INPUT, "");
		configInput(VOL_INPUT, "");
		configInput(CAVITYX_INPUT, "");
		configInput(CAVITYY_INPUT, "");
		configInput(TONGUEX_INPUT, "");
		configInput(TONGUEY_INPUT, "");
		configInput(SOFTPALATE_INPUT, "");
		configOutput(OUTPUT_OUTPUT, "");
	}


	float tongueX = 0.0;
	float tongueY = 0.0;
	float constrictionX = 0.0;
	float constrictionY = 0.0;
	float fricativeIntensity = 0.0;
	// bool muteAudio = false;
	bool constrictionActive = false;

	sample_t sampleRate = APP->engine->getSampleRate();
	sample_t samplesPerBlock = 1;
	int n = 44; //this is what the VST plugin used. Still no clue what it is.
	t_tractProps tractProps;
	// from https://github.com/cutelabnyc/pink-trombone-plugin/blob/master/Source/PluginProcessor.cpp
	Glottis * glottis = new Glottis(sampleRate);
	Tract * tract = new Tract(sampleRate, samplesPerBlock, &tractProps);
	WhiteNoise * whiteNoise = new WhiteNoise(sampleRate * 2.0);
	Biquad * aspirateFilter = new Biquad(sampleRate);
	Biquad * fricativeFilter = new Biquad(sampleRate);
	
	void process(const ProcessArgs& args) override {

		double purenoise = whiteNoise->runStep();
		double asp = aspirateFilter->runStep(purenoise);
		double fri = fricativeFilter->runStep(purenoise);
		
		// Glottis
		// the original code has a loop from j to n here that I *THINK* is accounting for the buffer, which isn't applicable here.
		// See line 181 of https://github.com/cutelabnyc/pink-trombone-plugin/blob/master/Source/PluginProcessor.cpp 
		// Until I understand this code better, I'm just overriding the values here.
		//double lambda1 = (double) j / (double) N;
		//double lambda2 = ((double) j + 0.5) / (double) N;
		double lambda1 = 0.5;
		double lambda2 = 0.5;
		
		double glot = glottis->runStep(lambda1, asp);
		double vocalOutput = 0.0;
		tract->runStep(glot, fri, lambda1, glot);
		vocalOutput += tract->lipOutput + tract->noseOutput;
		tract->runStep(glot, fri, lambda2, glot);
		vocalOutput += tract->lipOutput + tract->noseOutput;

		// I think vocal output is the actual audio out, so that should be good to go - however, it's a double, so it will need to be scaled down to a float with the correct voltage range.

		double tongueIndex = tongueX * ((double) (tract->tongueIndexUpperBound() - tract->tongueIndexLowerBound())) + tract->tongueIndexLowerBound();
		double innerTongueControlRadius = 2.05;
		double outerTongueControlRadius = 3.5;
		double tongueDiameter = tongueY * (outerTongueControlRadius - innerTongueControlRadius) + innerTongueControlRadius;
		double constrictionMin = -2.0;
		double constrictionMax = 2.0;

		double constrictionIndex = this->constrictionX * (double) this->tract->getTractIndexCount();
		double constrictionDiameter = this->constrictionY * (constrictionMax - constrictionMin) + constrictionMin;

		if (constrictionActive == false) {
			constrictionDiameter = constrictionMax;
		} else {
			fricativeIntensity += 0.1; // TODO ex recto
			fricativeIntensity = minf(1.0, this->fricativeIntensity);
		}

		tract->setRestDiameter(tongueIndex, tongueDiameter);
		tract->setConstriction(constrictionIndex, constrictionDiameter, fricativeIntensity);
		glottis->finishBlock();
		tract->finishBlock();

	}

	void onAdd() override {
		initializeTractProps(&tractProps, n);
		aspirateFilter->setGain(1.0);
		aspirateFilter->setQ(0.5);
		aspirateFilter->setFrequency(500);
		fricativeFilter->setGain(1.0);
		fricativeFilter->setQ(0.5);
		fricativeFilter->setFrequency(1000);
	}
};


struct PinkTromboneWidget : ModuleWidget {
	PinkTromboneWidget(PinkTrombone* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/pinktrombone.svg")));

		addChild(createWidget<Bolt>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<Bolt>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<Bolt>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<Bolt>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<HexKnob>(mm2px(Vec(17.0, 46.0)), module, PinkTrombone::PITCHO_PARAM));
		addParam(createParamCentered<SmallHexKnobInv>(mm2px(Vec(17.0, 46.0)), module, PinkTrombone::PITCHA_PARAM));
		addParam(createParamCentered<HexKnob>(mm2px(Vec(8.0, 49.941)), module, PinkTrombone::VOLO_PARAM));
		addParam(createParamCentered<SmallHexKnobInv>(mm2px(Vec(8.0, 49.941)), module, PinkTrombone::VOLA_PARAM));
		addParam(createParamCentered<HexKnob>(mm2px(Vec(17.0, 64.0)), module, PinkTrombone::CAVITYXO_PARAM));
		addParam(createParamCentered<SmallHexKnobInv>(mm2px(Vec(17.0, 64.0)), module, PinkTrombone::CAVITYXA_PARAM));
		addParam(createParamCentered<HexKnob>(mm2px(Vec(8.0, 67.941)), module, PinkTrombone::CAVITYYO_PARAM));
		addParam(createParamCentered<SmallHexKnobInv>(mm2px(Vec(8.0, 67.941)), module, PinkTrombone::CAVITYYA_PARAM));
		addParam(createParamCentered<HexKnob>(mm2px(Vec(17.0, 82.0)), module, PinkTrombone::TONGUEXO_PARAM));
		addParam(createParamCentered<SmallHexKnobInv>(mm2px(Vec(17.0, 82.0)), module, PinkTrombone::TOUNGEXA_PARAM));
		addParam(createParamCentered<HexKnob>(mm2px(Vec(8.0, 85.941)), module, PinkTrombone::TONGUEYO_PARAM));
		addParam(createParamCentered<SmallHexKnobInv>(mm2px(Vec(8.0, 85.941)), module, PinkTrombone::TOUNGEYA_PARAM));
		addParam(createParamCentered<CKD6>(mm2px(Vec(8.0, 104.0)), module, PinkTrombone::SOFTPALATE_PARAM));

		addInput(createInputCentered<InJack>(mm2px(Vec(33.0, 46.0)), module, PinkTrombone::PITCH_INPUT));
		addInput(createInputCentered<InJack>(mm2px(Vec(42.0, 50.0)), module, PinkTrombone::VOL_INPUT));
		addInput(createInputCentered<InJack>(mm2px(Vec(33.0, 64.0)), module, PinkTrombone::CAVITYX_INPUT));
		addInput(createInputCentered<InJack>(mm2px(Vec(42.0, 68.0)), module, PinkTrombone::CAVITYY_INPUT));
		addInput(createInputCentered<InJack>(mm2px(Vec(33.0, 82.0)), module, PinkTrombone::TONGUEX_INPUT));
		addInput(createInputCentered<InJack>(mm2px(Vec(42.0, 86.0)), module, PinkTrombone::TONGUEY_INPUT));
		addInput(createInputCentered<InJack>(mm2px(Vec(42.0, 104.0)), module, PinkTrombone::SOFTPALATE_INPUT));

		addOutput(createOutputCentered<OutJack>(mm2px(Vec(25.4, 121.0)), module, PinkTrombone::OUTPUT_OUTPUT));
	}
};


Model* modelPinkTrombone = createModel<PinkTrombone, PinkTromboneWidget>("PinkTrombone");