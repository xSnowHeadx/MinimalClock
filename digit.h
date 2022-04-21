/*
 * digit.h
 *
 *  Created on: 22.08.2021
 *      Author: Uwe
 */

#ifndef DIGIT_H_
#define DIGIT_H_

// settings
#define EIGHT_DIGIT 		false     	// true if your machine has 8 digits
#if EIGHT_DIGIT
#define DIGIT 8
#else
#define DIGIT 4
#endif

#define HAS_ENDSTOP			false		// true if the clock is a version with mechanical stop at "0000"
#if HAS_ENDSTOP
#define ENDSTOP_RELEASE 	-15			// release pushing force to align all digits better
#define DISP_POS 			  0			// positon of display. 0:front, 1;upper
#else
#define ORIGIN_SENSOR 		false   	// true if you installed origin sensor
#if ORIGIN_SENSOR
#define ORIGIN_COMPENSATION -390 		// compensation of origin mark position
#define ORIGIN_BRIGHTMARK	true		// true if origin mark is brighter than background
#define ORIGIN_THRES 		100      	// photo reflector sensor threshold
#endif
#endif

#define PRE_MOVE 			true		// true to start move earlier to reach the end position just in time

#define DEBUG 				true		// true for additional serial debug messages

#define POSITION 10 					// each wheel has 10 positions

typedef struct {
  int v[DIGIT];
} Digit;


#endif /* DIGIT_H_ */
