//
//  config.h
//  PinkTrombone
//
//  Created by Samuel Tarakajian on 4/5/21.
//

#ifndef config_h
#define config_h

// Change size of a sample
//typedef double sample_t;
typedef float sample_t;

// Tract properties
#define MAX_TRANSIENTS 			(20)
#define NUM_CONSTRICTIONS				(44.0)
#define BLADE_START				(10)
#define NOSE_LENGTH				(28)
#define TIP_START				(32)
#define	LIP_START				(39)
//#define NUM_CONSTRICTIONS				(22.0)
//#define BLADE_START				(5)
//#define NOSE_LENGTH				(14)
//#define TIP_START				(16)
//#define	LIP_START				(18)
#define TONGUE_DIAMETER			(3.5)
#define NOSE_OFFSET				(0.8)
#define GLOTTAL_REFLECTION		(0.75)
#define LIP_REFLECTION			(-0.85)
#define	TRACT_FADE				(1.0)
#define MOVEMENT_SPEED			(15)
#define TRACT_BOUND_A			(7.0 / 44.0)
#define TRACT_DIAMETER_A		(0.6)
#define TRACT_BOUND_B			(12.0 / 44.0)
#define TRACT_DIAMETER_B		(1.1)
#define TRACT_DIAMETER_C		(1.5)

// Glottis properties
#define VIBRATO_AMOUNT			(0.005)
//#define VIBRATO_AMOUNT			(0)
#define VIBRATO_FREQUENCY		(6)

#endif /* config_h */
