/*
 * blink.cpp
 *
 *  Created on: 13.02.2021
 *      Author: Stefan Fuss
 */

#include <board.h>
#include "blink.h"

void  blink( int highTime, int lowTime ) {

    /* Blink on (output high) */
    gpio_set_level((gpio_num_t)BLINK_GPIO, 1);
    vTaskDelay(highTime / portTICK_RATE_MS);

    /* Blink off (output low) */
    gpio_set_level((gpio_num_t)BLINK_GPIO, 0);
    vTaskDelay(lowTime / portTICK_RATE_MS);

}

void SOS( int errorCode )
{
	int i;

    while( 1 ) {

    	// S
    	for ( i=0; i<3; i++ ) {
    		blink( 125, 250 );
    	}

    	// O
    	for ( i=0; i<3; i++ ) {
    		blink( 500, 250 );
    	}

    	// S
    	for ( i=0; i<3; i++ ) {
    		blink( 125, 250 );
    	}

    	// wait short time
    	vTaskDelay( 1000 / portTICK_RATE_MS);

    	for ( int i=0; i<errorCode; i++){
    		blink( 250, 250 );
    	}

    	// wait short time
    	vTaskDelay( 1000 / portTICK_RATE_MS);

    }

}



