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

	dsp::ClockDivider processDivider;

	float 				pitch = dsp::FREQ_C4;
    float               tongueX = 0.0;
    float               tongueY = 0.0;
    float               constrictionX = 0.0;
    float               constrictionY = 0.0;
    float               fricativeIntensity = 0.0;
    // bool             muteAudio = false;
    bool                constrictionActive = false;

    sample_t            sampleRate = 44100;
    sample_t            samplesPerBlock = 1;
    int                  n = 44; //this is what the VST plugin used. Still no clue what it is.
    t_tractProps        tractProps;
    
    // from https://github.com/cutelabnyc/pink-trombone-plugin/blob/master/Source/PluginProcessor.cpp
    Glottis             *glottis = NULL;
    Tract               *tract = NULL;
    WhiteNoise          *whiteNoise = NULL;
    Biquad              *aspirateFilter = NULL;
    Biquad              *fricativeFilter = NULL;

    bool                destroying = false;
    
	uint32_t            m_buffer_size = 128;
	// uint32_t            m_buffer_phase = 0;
	// float               m_buffer_A[128] = {};
	// float               m_buffer_B[128] = {};
	// float               *m_filling_buffer = NULL;
	// float               *m_output_buffer = NULL;

	PinkTrombone() {
        
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		
        // configParam(PITCHO_PARAM, 0.1, 1.f, 0.1, "Pitch Offset");
		configParam(PITCHO_PARAM, -4.f, 4.f, 0.f, "Frequency Offset", " Hz", 2, dsp::FREQ_C4);
		configParam(PITCHA_PARAM, 0.f, 1.f, 1.f, "Pitch Attenuation");
		configParam(VOLO_PARAM, 0.f, 1.f, 0.f, "VCA Offset");
		configParam(VOLA_PARAM, 0.f, 1.f, 0.f, "VCA Attenuation");
		configParam(CAVITYXO_PARAM, 0.f, 1.f, 0.f, "Cavity X Offset");
		configParam(CAVITYXA_PARAM, 0.f, 1.f, 0.f, "Cavity X Attenuation");
		configParam(CAVITYYO_PARAM, 0.f, 1.f, 0.f, "Cavity Y Offset");
		configParam(CAVITYYA_PARAM, 0.f, 1.f, 0.f, "Cavity Y Attenuation");
		configParam(TONGUEXO_PARAM, 0.f, 1.f, 0.f, "Tongue X Offset");
		configParam(TOUNGEXA_PARAM, 0.f, 1.f, 0.f, "Tounge X Attenuation");
		configParam(TONGUEYO_PARAM, 0.f, 1.f, 0.f, "Tounge Y Offset");
		configParam(TOUNGEYA_PARAM, 0.f, 1.f, 0.f, "Tounge Y Attenuation");
		configParam(SOFTPALATE_PARAM, 0.f, 1.f, 0.f, "Soft Palate Toggle");
		configInput(PITCH_INPUT, "Pitch V/Oct");
		configInput(VOL_INPUT, "VCA Control");
		configInput(CAVITYX_INPUT, "Cavity X");
		configInput(CAVITYY_INPUT, "Cavity Y");
		configInput(TONGUEX_INPUT, "Tongue X");
		configInput(TONGUEY_INPUT, "Tongue Y");
		configInput(SOFTPALATE_INPUT, "Soft Palate Gate");
		configOutput(OUTPUT_OUTPUT, "Output");

        sampleRate = APP->engine->getSampleRate();
        samplesPerBlock = m_buffer_size;
        n = 44; //this is what the VST plugin used. Still no clue what it is.
        
        initializeTractProps(&tractProps, n);

        glottis = new Glottis(sampleRate);
        
        tract = new Tract(sampleRate, samplesPerBlock, &tractProps);
        
        whiteNoise = new WhiteNoise(sampleRate * 2.0);
        
        aspirateFilter = new Biquad(sampleRate);
        aspirateFilter->setGain(1.0);
        aspirateFilter->setQ(0.5);
        aspirateFilter->setFrequency(500);
        
        fricativeFilter = new Biquad(sampleRate);
        fricativeFilter->setGain(1.0);
        fricativeFilter->setQ(0.5);
        fricativeFilter->setFrequency(1000);

		processDivider.setDivision(64);
        
        // m_filling_buffer = m_buffer_A;
        // m_output_buffer = m_buffer_B;
        // m_buffer_phase = 0;
	}

    virtual ~PinkTrombone() {
        destroying = true;
        
        delete fricativeFilter;
        delete aspirateFilter;
        delete whiteNoise;
        delete glottis;
        delete tract;
        
        destroyTractProps(&tractProps);

    }
	
	void process(const ProcessArgs& args) override {

        if(destroying)
            return;
        
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


        tongueX = params[TONGUEXO_PARAM].getValue() + (params[TOUNGEXA_PARAM].getValue() * inputs[TONGUEX_INPUT].getVoltage());
        tongueY = params[TONGUEYO_PARAM].getValue() + (params[TOUNGEYA_PARAM].getValue() * inputs[TONGUEY_INPUT].getVoltage());
        constrictionX = params[CAVITYXO_PARAM].getValue() + (params[CAVITYXA_PARAM].getValue() * inputs[CAVITYX_INPUT].getVoltage());
        constrictionY = params[CAVITYYO_PARAM].getValue() + (params[CAVITYYA_PARAM].getValue() * inputs[CAVITYY_INPUT].getVoltage());
        
        fricativeIntensity = params[CAVITYYO_PARAM].getValue() + params[CAVITYYO_PARAM].getValue() * inputs[SOFTPALATE_INPUT].getVoltage();

		// glottis->setTargetFrequency(params[PITCHO_PARAM].getValue() * 1000.0);

        double tongueIndex = tongueX * ((double) (tract->tongueIndexUpperBound() - tract->tongueIndexLowerBound())) + tract->tongueIndexLowerBound();
		double innerTongueControlRadius = 2.05;
		double outerTongueControlRadius = 3.5;
		double tongueDiameter = tongueY * (outerTongueControlRadius - innerTongueControlRadius) + innerTongueControlRadius;
		double constrictionMin = -2.0;
		double constrictionMax = 2.0;

		double constrictionIndex = constrictionX * (double) tract->getTractIndexCount();
		double constrictionDiameter = constrictionY * (constrictionMax - constrictionMin) + constrictionMin;

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

        
        outputs[OUTPUT_OUTPUT].setVoltage(vocalOutput);
        // deal with output
//        m_filling_buffer[m_buffer_phase] = vocalOutput;
//        outputs[OUTPUT_OUTPUT].setVoltage(m_output_buffer[m_buffer_phase]);
//        m_buffer_phase++;
//        if(m_buffer_phase >= m_buffer_size)
//        {
//            m_buffer_phase = 0;
//            float    *tmp = m_filling_buffer;
//            m_filling_buffer = m_output_buffer;
//            m_output_buffer = tmp;
//        }
        
        
//        uint32_t            m_buffer_size = 128;
//        uint32_t            m_buffer_phase = 0;
//        float               m_buffer_A[128] = {};
//        float               m_buffer_B[128] = {};
//        float               *m_filling_buffer = NULL;
//        float               *m_output_buffer = NULL;

	if (processDivider.process()){
		pitch = dsp::FREQ_C4 + params[PITCHO_PARAM].getValue() * dsp::FREQ_C4;
		pitch = pitch * pow(2.0, params[PITCHA_PARAM].getValue() * inputs[PITCH_INPUT].getVoltage());
        glottis->setTargetFrequency(pitch);
	}

	}

	void onAdd() override {
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
