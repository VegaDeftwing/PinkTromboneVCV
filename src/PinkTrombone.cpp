#include "plugin.hpp"

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
		TONGUEX_PARAM,
		TONGUEY_PARAM,
		NOSE_PARAM,
		//LIP_PARAM,
		THROATX_PARAM,
		THROATY_PARAM,
		TONGUEXA_PARAM,
		TONGUEYA_PARAM,
		THROATXA_PARAM,
		THROATYA_PARAM,
		TIP_PARAM,
		BLADE_PARAM,
		FRICFC_PARAM,
		FRICFCA_PARAM,
		FRICQ_PARAM,
		FRICQA_PARAM,
		FRICL_PARAM,
		FRICLA_PARAM,
		FCFOLLOW_PARAM,
		TENSEA_PARAM,
		TENSE_PARAM,
		VOCTA_PARAM,
		MASTERPITCH_PARAM,
		FMA_PARAM,
		VIBA_PARAM,
		VIB_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TONGUEXI_INPUT,
		TONGUEYI_INPUT,
		THROATXI_INPUT,
		THROATYI_INPUT,
		FRICFCI_INPUT,
		FRICQI_INPUT,
		FRICL_INPUT,
		TENSEI_INPUT,
		VOCT_INPUT,
		FMI_INPUT,
		VIBI_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(MAIN_LIGHT, 3),
		ENUMS(U_LIGHT, 3),
		ENUMS(D_LIGHT, 3),
		ENUMS(L_LIGHT, 3),
		ENUMS(R_LIGHT, 3),
		LIGHTS_LEN
	};

	dsp::ClockDivider processDivider;

	float pitch = dsp::FREQ_C4;
	float fm = 0;
	float tongueX = 0.0;
	float tongueY = 0.0;
	float constrictionX = 0.0;
	float constrictionY = 0.0;
	float fricativeIntensity = 0.0;
	// bool             muteAudio = false;
	bool constrictionActive = true;

	sample_t sampleRate = 44100;
	sample_t samplesPerBlock = 1;
	int n = 44; // this is what the VST plugin used. Still no clue what it is.
	t_tractProps tractProps;

	// from https://github.com/cutelabnyc/pink-trombone-plugin/blob/master/Source/PluginProcessor.cpp
	Glottis *glottis = NULL;
	Tract *tract = NULL;
	WhiteNoise *whiteNoise = NULL;
	Biquad *aspirateFilter = NULL;
	Biquad *fricativeFilter = NULL;

	bool destroying = false;

	uint32_t m_buffer_size = 128;
	// uint32_t            m_buffer_phase = 0;
	// float               m_buffer_A[128] = {};
	// float               m_buffer_B[128] = {};
	// float               *m_filling_buffer = NULL;

	PinkTrombone()
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TONGUEX_PARAM, 0.f, 1.f, 0.f, "Tongue X");
		configParam(TONGUEY_PARAM, 0.f, 1.f, 0.f, "Tongue Y");
		configParam(NOSE_PARAM, 1.f, 10.f, 4.1, "Nose");
		configParam(TENSE_PARAM, 0.f, 1.f, 0.f, "Tense");
		configParam(THROATX_PARAM, 0.06, 0.999, 0.06, "Throat X");
		configParam(THROATY_PARAM,  0.575, 0.69125, 0.69125, "Throat Y");
		configParam(TONGUEXA_PARAM, -1.f, 1.f, 0.f, "Tongue X Attenuversion");
		configParam(TONGUEYA_PARAM, -1.f, 1.f, 0.f, "Tongue Y Attenuversion");
		configParam(THROATXA_PARAM, -1.f, 1.f, 0.f, "Throat X Attenuversion");
		configParam(THROATYA_PARAM, -1.f, 1.f, 0.f, "Threat Y Attenuversion");
		configParam(TIP_PARAM, 0.f, 9.f, 2.2, "Tip");
		// configParam(LIP_PARAM, 0.f, 9.f, 0.f, "Lip");
		configParam(BLADE_PARAM, 0.f, 1.f, 0.87, "Blade");
		configParam(FRICFC_PARAM, 0.f, 1.f, 0.5, "Fricative & Aspiration Fc");
		configParam(FRICFCA_PARAM, -1.f, 1.f, 0.f, "Fricative & Aspiration Fc Attenuversion");
		configParam(FRICQ_PARAM, 1.2, 0.00001, 0.5, "Frciative & Aspiration Q");
		configParam(FRICQA_PARAM, -1.f, 1.f, 0.f, "Fricative Q Attenuversion");
		configParam(FRICL_PARAM, 0.f, 1.f, 0.7325, "Fricative Level");
		configParam(FRICLA_PARAM, -1.f, 1.f, 0.f, "Fricative Level Attenuversion");
		configParam(FCFOLLOW_PARAM, 0.f, 1.f, 0.f, "Fricative and Aspirate Fc Follow Master Pitch");
		configParam(TENSEA_PARAM, 0.f, 1.f, 0.f, "Tense Attenuversion");
		configParam(TENSE_PARAM, 0.f, 1.f, 0.44, "Tenseness");
		configParam(VOCTA_PARAM, 0.f, 1.f, 1.f, "V/Oct Attenuversion");
		configParam(MASTERPITCH_PARAM, -0.9, 4.f, 0.f, "Master Pitch", " Hz", 2, dsp::FREQ_C4); // range to prevent lockups
		configParam(FMA_PARAM, -1.f, 1.f, 0.f, "FM Attenuversion");
		configParam(VIBA_PARAM, 0.f, 1.f, 0.f, "Vibrato Attenuversion");
		configParam(VIB_PARAM, 0.f, 1.f, 0.35, "Vibrato");
		configInput(TONGUEXI_INPUT, "Tounge X Input");
		configInput(TONGUEYI_INPUT, "Tounge Y Input");
		configInput(THROATXI_INPUT, "Throat X Input");
		configInput(THROATYI_INPUT, "Throat Y Input");
		configInput(FRICFCI_INPUT, "Fricative Fc Input");
		configInput(FRICQI_INPUT, "Frciative Q Input");
		configInput(FRICL_INPUT, "Frciative Level Input");
		configInput(TENSEI_INPUT, "Tenseness Input");
		configInput(VOCT_INPUT, "VOct Input");
		configInput(FMI_INPUT, "FM Input");
		configInput(VIBI_INPUT, "Vibrato Input");
		configOutput(OUTPUT_OUTPUT, "Output");

		sampleRate = APP->engine->getSampleRate();
		samplesPerBlock = m_buffer_size;
		n = 44; // this is what the VST plugin used. Still no clue what it is.

		initializeTractProps(&tractProps, n);

		glottis = new Glottis(sampleRate);

		tract = new Tract(sampleRate, samplesPerBlock, &tractProps);

		whiteNoise = new WhiteNoise(sampleRate * 2.0);

		aspirateFilter = new Biquad(sampleRate);
		aspirateFilter->setGain(1.0);
		aspirateFilter->setQ(0.5);
		aspirateFilter->setFrequency(500);
		// aspirateFilter->setQ(0.5);
		// aspirateFilter->setFrequency(100);

		fricativeFilter = new Biquad(sampleRate);
		fricativeFilter->setGain(1.0);
		fricativeFilter->setQ(0.5);
		fricativeFilter->setFrequency(1000);

		processDivider.setDivision(64);

		// m_filling_buffer = m_buffer_A;
		// m_output_buffer = m_buffer_B;
		// m_buffer_phase = 0;
	}

	virtual ~PinkTrombone()
	{
		destroying = true;

		delete fricativeFilter;
		delete aspirateFilter;
		delete whiteNoise;
		delete glottis;
		delete tract;

		destroyTractProps(&tractProps);
	}

	void process(const ProcessArgs &args) override
	{

		if (destroying)
			return;

		float fricativeLevel = params[FRICL_PARAM].getValue()+(params[FRICLA_PARAM].getValue()*(inputs[FRICL_INPUT].getVoltage()/10.f)); 
		glottis->setIntensity(fricativeLevel);

		double purenoise = whiteNoise->runStep();
		double asp = aspirateFilter->runStep(purenoise);
		double fri = fricativeFilter->runStep(purenoise);

		float masterToFilter = dsp::FREQ_C4 + params[MASTERPITCH_PARAM].getValue() * dsp::FREQ_C4;
		masterToFilter *= pow(2.0, params[VOCTA_PARAM].getValue() * inputs[VOCT_INPUT].getVoltage());
		masterToFilter *= params[FCFOLLOW_PARAM].getValue();

		// I have these seperate in case it's desirable to to have the filters have a different cut off relative to one another later.
		float newFricFC = rack::clamp(params[FRICFC_PARAM].getValue() * 1000.f + (params[FRICFCA_PARAM].getValue() * (inputs[FRICFCI_INPUT].getVoltage() * 2000.f)) + masterToFilter, 0.f, sampleRate/2.01);
		float newAspFC = rack::clamp(params[FRICFC_PARAM].getValue() * 1000.f + (params[FRICFCA_PARAM].getValue() * (inputs[FRICFCI_INPUT].getVoltage() * 2000.f)) + masterToFilter, 0.f, sampleRate/2.01);
		
		float newQ = rack::clamp(params[FRICQ_PARAM].getValue() + (params[FRICQA_PARAM].getValue() * (inputs[FRICQI_INPUT].getVoltage())), 0.00001, 1.5);

		fricativeFilter->setFrequency(newFricFC);
		fricativeFilter->setQ(newQ);

		aspirateFilter->setFrequency(newAspFC);
		aspirateFilter->setQ(newQ);

		lights[U_LIGHT + 0].setBrightness(newAspFC / (sampleRate/20.0));
		lights[U_LIGHT + 1].setBrightness(1.2-newQ);
		lights[U_LIGHT + 2].setBrightness(fricativeLevel);

		// int lipStart; -- Nothing happens.
		// int bladeStart; -- This one works
		// int tipStart; -- This works, but crashes depending on the postion of blade start, hence the code below
		// int noseStart; -- This one works
		// int noseLength; -- Trying to expose this causes a crash so hard I loose my mouse and have to restart Jack Audio, so, NOPE.
		// sample_t noseOffset; -- Nope.
		// sample_t tongueIndex; -- Nope.
		// sample_t tongueDiameter; -- Nope.
		// sample_t *noseDiameter; -- Nope.
		// sample_t *tractDiameter; -- Nope.
		// tractProps.lipStart = (int)(params[LIP_PARAM].getValue());
		// float noiseDia = params[NOSE_PARAM].getValue() * 100.f;
		// *tractProps.noseDiameter = noiseDia;

		// tractProps.noseLength = (int)(params[LIP_PARAM].getValue()*10.f);
		tractProps.noseStart = (int)(params[NOSE_PARAM].getValue());
		tractProps.bladeStart = (int)(1.f + params[BLADE_PARAM].getValue() * 10.f);
		tractProps.tipStart = (int)(params[TIP_PARAM].getValue()*params[BLADE_PARAM].getValue());


		// Glottis
		// the original code has a loop from j to n here that I *THINK* is accounting for the buffer, which isn't applicable here.
		// See line 181 of https://github.com/cutelabnyc/pink-trombone-plugin/blob/master/Source/PluginProcessor.cpp
		// Until I understand this code better, I'm just overriding the values here.
		// double lambda1 = (double) j / (double) N;
		// double lambda2 = ((double) j + 0.5) / (double) N;
		// As far as I can tell, these have no impact and can **probably** be removed. For now I'm leaving them in.
		double lambda1 = 0;
		double lambda2 = 0;

		double glot = glottis->runStep(lambda1, asp);
		double vocalOutput = 0.0;
		tract->runStep(glot, fri, lambda1, glot);
		vocalOutput += tract->lipOutput + tract->noseOutput;
		tract->runStep(glot, fri, lambda2, glot);
		vocalOutput += tract->lipOutput + tract->noseOutput;

		// I think vocal output is the actual audio out, so that should be good to go - however, it's a double, so it will need to be scaled down to a float with the correct voltage range.

		tongueX = params[TONGUEX_PARAM].getValue() + (params[TONGUEXA_PARAM].getValue() * (inputs[TONGUEXI_INPUT].getVoltage()/20.f));
		tongueY = params[TONGUEY_PARAM].getValue() + (params[TONGUEYA_PARAM].getValue() * (inputs[TONGUEYI_INPUT].getVoltage()/20.f));
		lights[L_LIGHT + 0].setBrightness(tongueX);
		lights[L_LIGHT + 2].setBrightness(tongueY);
		lights[L_LIGHT + 1].setBrightness(tongueX*tongueY);
		// Constriction X has some impact, but not as much as I think it should.
		constrictionX = params[THROATX_PARAM].getValue() + (params[THROATXA_PARAM].getValue() * (inputs[THROATXI_INPUT].getVoltage()/20.f));
		constrictionY = params[THROATY_PARAM].getValue() + (params[THROATYA_PARAM].getValue() * (inputs[THROATYI_INPUT].getVoltage()/20.f));
		lights[R_LIGHT + 0].setBrightness(constrictionX*1.f);
		lights[R_LIGHT + 2].setBrightness((constrictionY*20.f)-(5.75*2.f));
		lights[R_LIGHT + 1].setBrightness((constrictionX*1.f)*(constrictionY*20.f)-(5.75*2.f));

		// fricativeIntensity = params[CAVITYYO_PARAM].getValue() + params[CAVITYYO_PARAM].getValue() * inputs[SOFTPALATE_INPUT].getVoltage();
		//  This affects the amount of noise being added when the throat is nearly closed (Cavity Y < ~.67). This should be exposed on the panel.
		fricativeIntensity = 1.f;

		double tongueIndex = tongueX * ((double)(tract->tongueIndexUpperBound() - tract->tongueIndexLowerBound())) + tract->tongueIndexLowerBound();
		double innerTongueControlRadius = 2.05;
		double outerTongueControlRadius = 3.5;
		double tongueDiameter = tongueY * (outerTongueControlRadius - innerTongueControlRadius) + innerTongueControlRadius;
		double constrictionMin = -2.0;
		double constrictionMax = 2.0;

		double constrictionIndex = constrictionX * (double)tract->getTractIndexCount();
		double constrictionDiameter = constrictionY * (constrictionMax - constrictionMin) + constrictionMin;

		if (constrictionActive == false)
		{
			constrictionDiameter = constrictionMax;
		}
		else
		{
			fricativeIntensity += 0.1; // TODO ex recto
			fricativeIntensity = minf(1.0, this->fricativeIntensity);
		}
		//TODO: The high CPU usage is possibly from these two:
		tract->setRestDiameter(tongueIndex, tongueDiameter);
		tract->setConstriction(constrictionIndex, constrictionDiameter, fricativeIntensity);
		
		float vibAmount = params[VIB_PARAM].getValue()+(params[VIBA_PARAM].getValue() * inputs[VIBI_INPUT].getVoltage());
		glottis->finishBlock(vibAmount); //This argument is the amount of vibrato
		tract->finishBlock();

		// It likes to crash in a fun "Lets output an wave of alternating +1000000 -1000000 samples that seek to destory all ears in a 10 mile radius. So, clamping.
		outputs[OUTPUT_OUTPUT].setVoltage(rack::clamp((float)vocalOutput, (float)-10.0, (float)10.0));

		if (processDivider.process())
		{
			pitch = dsp::FREQ_C4 + params[MASTERPITCH_PARAM].getValue() * dsp::FREQ_C4;
			pitch = pitch * pow(2.0, params[VOCTA_PARAM].getValue() * inputs[VOCT_INPUT].getVoltage());
			fm = (params[FMA_PARAM].getValue() * inputs[FMI_INPUT].getVoltage()) * dsp::FREQ_C4;
			glottis->setTargetFrequency(rack::clamp(pitch+fm, (float)2., (float)dsp::FREQ_C4*10.));
			// misunderstanding here, tenseness is *not* volume. There must be another parameter for volume somewhere...
			float tenseness = rack::clamp(params[TENSE_PARAM].getValue() + (params[TENSEA_PARAM].getValue() * (inputs[TENSEI_INPUT].getVoltage()/10.f)), 0.f, 1.f);
			glottis->setTargetTenseness(tenseness);
			lights[MAIN_LIGHT + 0].setBrightness(pitch/(sampleRate/16.0));
			lights[MAIN_LIGHT + 1].setBrightness(fm);
			lights[MAIN_LIGHT + 2].setBrightness(vibAmount);

			lights[D_LIGHT + 0].setBrightness(tenseness);
			lights[D_LIGHT + 1].setBrightness(tract->lipOutput);
			lights[D_LIGHT + 2].setBrightness(tract->noseOutput);
		}
	}

	void onAdd() override
	{
	}
};

struct PinkTromboneWidget : ModuleWidget
{
	PinkTromboneWidget(PinkTrombone* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PinkTrombone.svg")));

		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(9.982, 11.816)), module, PinkTrombone::TONGUEX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.482, 11.816)), module, PinkTrombone::TONGUEY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.581, 18.708)), module, PinkTrombone::NOSE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(90.581, 18.708)), module, PinkTrombone::LIP_PARAM));
		addChild(createLightCentered<LargeLight<RedGreenBlueLight>>(mm2px(Vec(90.581, 18.708)), module, PinkTrombone::MAIN_LIGHT));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(95.581, 18.708)), module, PinkTrombone::R_LIGHT));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(85.581, 18.708)), module, PinkTrombone::L_LIGHT));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(90.581, 13.708)), module, PinkTrombone::U_LIGHT));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(90.581, 23.708)), module, PinkTrombone::D_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(40.982, 19.816)), module, PinkTrombone::THROATX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(56.482, 19.816)), module, PinkTrombone::THROATY_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(9.982, 26.816)), module, PinkTrombone::TONGUEXA_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(25.482, 26.816)), module, PinkTrombone::TONGUEYA_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(40.982, 34.816)), module, PinkTrombone::THROATXA_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(56.482, 34.816)), module, PinkTrombone::THROATYA_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(76.03, 40.315)), module, PinkTrombone::TIP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(91.03, 40.315)), module, PinkTrombone::BLADE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(21.595, 59.982)), module, PinkTrombone::FRICFC_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(21.595, 59.982)), module, PinkTrombone::FRICFCA_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(45.595, 59.982)), module, PinkTrombone::FRICQ_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(45.595, 59.982)), module, PinkTrombone::FRICQA_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(69.595, 59.982)), module, PinkTrombone::FRICL_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(69.595, 59.982)), module, PinkTrombone::FRICLA_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.696, 77.263)), module, PinkTrombone::FCFOLLOW_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(40.822, 76.971)), module, PinkTrombone::TENSEA_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(53.822, 76.971)), module, PinkTrombone::TENSE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(19.457, 102.549)), module, PinkTrombone::VOCTA_PARAM));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(20.557, 117.736)), module, PinkTrombone::MASTERPITCH_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(32.719, 118.936)), module, PinkTrombone::FMA_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(72.872, 118.936)), module, PinkTrombone::VIBA_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(85.872, 118.936)), module, PinkTrombone::VIB_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.982, 36.816)), module, PinkTrombone::TONGUEXI_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.482, 36.816)), module, PinkTrombone::TONGUEYI_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40.982, 44.816)), module, PinkTrombone::THROATXI_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(56.482, 44.816)), module, PinkTrombone::THROATYI_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.595, 59.982)), module, PinkTrombone::FRICFCI_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34.595, 59.982)), module, PinkTrombone::FRICQI_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.595, 59.982)), module, PinkTrombone::FRICL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.822, 76.971)), module, PinkTrombone::TENSEI_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.457, 92.996)), module, PinkTrombone::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(42.872, 118.936)), module, PinkTrombone::FMI_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(63.872, 118.936)), module, PinkTrombone::VIBI_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(88.022, 93.939)), module, PinkTrombone::OUTPUT_OUTPUT));
	}
};

Model *modelPinkTrombone = createModel<PinkTrombone, PinkTromboneWidget>("PinkTrombone");