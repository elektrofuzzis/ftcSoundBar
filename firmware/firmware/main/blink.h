/*
 * blink.h
 *
 *  Created on: 13.02.2021
 *      Author: Stefan Fuss
 */

#ifndef MAIN_BLINK_H_
#define MAIN_BLINK_H_

#define BLINK_GPIO GPIO_NUM_22

void  blink( int highTime, int lowTime );

void SOS( int errorCode );

#endif /* MAIN_BLINK_H_ */
