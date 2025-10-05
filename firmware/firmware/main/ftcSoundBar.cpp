/*
 * ftcSoundBar.cpp
 *
 *  Created on: 10.01.2021
 *      Author: Stefan Fuss
 */

#include <esp_log.h>
#include <audio_element.h>
#include <String.h>
#include <stdio.h>
#include <sys/stat.h>

#include "adfcorrections.h"
#include "playlist.h"
#include "pipeline.h"
#include "ftcSoundBar.h"

#define TAGFTCSOUNDBAR "::ftcSoundBar"

FtcSoundBar::FtcSoundBar( ) {

	ESP_LOGD(TAGFTCSOUNDBAR, "constructor" );
	xBlinky = NULL;

	strcpy( WIFI_SSID, "YOUR_SSID");
	strcpy( WIFI_PASSWORD, "YOUR_PASSWORD");

	TXT_AP_MODE = false;
	I2C_MODE = false;
	WIFI = true;

	strcpy( HOSTNAME, "ftcSoundBar" );

	STARTUP_VOLUME = 15;

}

void FtcSoundBar::writeConfigFile( char *configFile )
{
	FILE* f;

   	f = fopen(configFile, "w");

    if (f == NULL) {
    	ESP_LOGE(TAGFTCSOUNDBAR, "Could not write %s.", configFile);
    	return;
    }

    fprintf( f, "WIFI=%d\n", WIFI);
    fprintf( f, "WIFI_SSID=%s\n", WIFI_SSID);
    fprintf( f, "WIFI_PASSWORD=%s\n", WIFI_PASSWORD);
    fprintf( f, "TXT_AP_MODE=%d\n", TXT_AP_MODE);
    fprintf( f, "I2C_MODE=%d\n", I2C_MODE);
    fprintf( f, "DEBUG=%d\n", DEBUG);
    fprintf( f, "STARTUP_VOLUME=%d\n", STARTUP_VOLUME);
    fprintf( f, "HOSTNAME=%s\n", HOSTNAME);

    fclose(f);

}

void FtcSoundBar::readConfigFile( char *configFile )
{   FILE* f;

    struct stat st;

    if (stat(configFile, &st) == 0) {
    	// CONFIG_FILE exists, start reading it
    	f = fopen(configFile, "r");

    	if (f == NULL) {
    		ESP_LOGE(TAGFTCSOUNDBAR, "Could not read %s.", configFile);
    	    return;
    	}

    	char line[256];
    	char key[256];
    	char value[256];

    	while ( fscanf( f, "%s\n", line ) > 0 ) {
    		strcpy( key, strtok( line, "=" ) );
    		strcpy( value, strtok( NULL, "=" ) );

    		if ( strcmp( key, "WIFI_SSID" ) == 0 ) {

    			memcpy( WIFI_SSID, value, 32 );

    		} else if ( strcmp( key, "WIFI_PASSWORD" ) == 0 ) {

    			memcpy( WIFI_PASSWORD, value, 64 );

    		} else if ( strcmp( key, "WIFI" ) == 0 ) {

    		    WIFI = ( atoi( value ) != 0 );

			} else if ( strcmp( key, "TXT_AP_MODE" ) == 0 ) {

    		    TXT_AP_MODE = ( atoi( value ) != 0 );

    		} else if ( strcmp( key, "STARTUP_VOLUME" ) == 0 ) {

    			STARTUP_VOLUME = atoi( value );

    		} else if ( strcmp( key, "I2C_MODE" ) == 0 ) {

    			I2C_MODE = atoi( value );

    		} else if ( strcmp( key, "HOSTNAME" ) == 0 ) {

    			strcpy( HOSTNAME, value );

    		} else if ( strcmp( key, "DEBUG" ) == 0 ) {

    			DEBUG = atoi( value );

    		} else {

    			ESP_LOGW(TAGFTCSOUNDBAR, "reading config file, ignoring pair (%s=%s)\n", key, value);
    		}

    	}

        fclose(f);

    } else {

    	// CONFIG_FILE is missing, create a sample file
    	ESP_LOGE(TAGFTCSOUNDBAR, "%s not found. Creating empty config file. Please set your parameters!", configFile);

    	writeConfigFile( configFile );

    }

}
