#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;
extern Model* modelPinkTrombone;

// Declare each Model, defined in each module source file
// extern Model* modelMyModule;

struct Bolt : SvgScrew {
	Bolt() {
		sw->svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/Bolt.svg"));
		sw->wrap();
		box.size = sw->box.size;
	}
};

struct HexKnob : app::SvgKnob {
    HexKnob() {
        minAngle = -0.8 * M_PI;
        maxAngle = 0.8 * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/HexKnob.svg")));
    }
};

struct SnappingHexKnob : app::SvgKnob {
    SnappingHexKnob() {
        minAngle = -0.8 * M_PI;
        maxAngle = 0.8 * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/HexKnob.svg")));
        snap = true;
        //snapValue = 1;
    }
};

struct SmallHexKnob : app::SvgKnob {
    SmallHexKnob() {
        minAngle = -0.8 * M_PI;
        maxAngle = 0.8 * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallHexKnob.svg")));
    }
};

struct SmallHexKnobInv : app::SvgKnob {
    SmallHexKnobInv() {
        minAngle = -0.8 * M_PI;
        maxAngle = 0.8 * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallHexKnobInverted.svg")));
    }
};

struct MedHexKnob : app::SvgKnob {
    MedHexKnob() {
        minAngle = -0.8 * M_PI;
        maxAngle = 0.8 * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MedHexKnob.svg")));
    }
};

struct InJack : app::SvgPort {
    widget::TransformWidget* tw;

    InJack() {
        // remove the widget from the frame buffer
        fb->removeChild(sw);
        // make a new Transform Widget
		tw = new TransformWidget();
        // add the SvgWidget to the TransformWidget
		tw->addChild(sw);
        // add the Transfrom widget to the frame buffer
		fb->addChild(tw);
        // finally set the svg that's actually loaded
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Jack.svg")));
        // make the transform widget's box.size be the same as the SvgWidget's
        tw->box.size = sw->box.size;
        // get a random angle of ratation. uniform gives a float from 0.0 to 1.0,
        // so that needs to be multiplied by pi to get enough range
		float angle = random::uniform() * M_PI;
        // set the transform matrix to the identity matrix to start
		tw->identity();
		// get the center of the widget as it's supposed to be, this is needed
        // as doing just tw->rotate(angle); does *not* rotate about the center
        // but instead about the top left corner. So, first we traslate by the
        // center, making the top left corner the center...
		math::Vec center = sw->box.getCenter();
		tw->translate(center);
        // ... do the rotation
		tw->rotate(angle);
        // ... and then undo the translation to put it back in the center
		tw->translate(center.neg());
    }
};

struct OutJack : app::SvgPort {
    widget::TransformWidget* tw;

    OutJack() {
        // see above comments to understand this
        fb->removeChild(sw);
		tw = new TransformWidget();
		tw->addChild(sw);
		fb->addChild(tw);
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Jack.svg")));
        tw->box.size = sw->box.size;
		box.size = tw->box.size;
		float angle = random::uniform() * M_PI;
		tw->identity();
		math::Vec center = sw->box.getCenter();
		tw->translate(center);
		tw->rotate(angle);
		tw->translate(center.neg());
    }
};