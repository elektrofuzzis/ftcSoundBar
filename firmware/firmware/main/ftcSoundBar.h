/*
 * ftcSoundBar.h
 *
 *  Created on: 10.01.2021
 *      Author: Stefan Fuss
 */


#ifndef MAIN_FTCSOUNDBAR_H_
#define MAIN_FTCSOUNDBAR_H_

#include "pipeline.h"

class FtcSoundBar {
public:

	Pipeline pipeline;
	char WIFI_SSID[32];
	char WIFI_PASSWORD[64];
	bool TXT_AP_MODE;
	bool I2C_MODE;
	bool DEBUG;
	char HOSTNAME[64];
	uint8_t STARTUP_VOLUME;

	TaskHandle_t xBlinky;

	FtcSoundBar();
	void readConfigFile(char *configFile);
	void writeConfigFile(char *configFile);

};

#endif
