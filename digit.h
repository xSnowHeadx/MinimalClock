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
#define ORIGIN_SENSOR 		false   	// true if you installed origin sensor
#define ORIGIN_COMPENSATION 50 			// compensation of origin mark position
#define ORIGIN_THRES 		3500      	// photo reflector sensor threshold
#define PRE_MOVE 			true		// true to start move earlier to reach the end position just in time
#define DEBUG 				true		// true for additional serial debug messages

#if EIGHT_DIGIT
#define DIGIT 8
#else
#define DIGIT 4
#endif

typedef struct {
  int v[DIGIT];
} Digit;


#endif /* DIGIT_H_ */
