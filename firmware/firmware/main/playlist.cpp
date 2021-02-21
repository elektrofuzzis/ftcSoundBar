/*
 * playlist.cpp
 *
 *  Created on: 09.12.2020
 *      Author: Stefan
 */

#include <playlist.h>
#include <dirent.h>
#include <string.h>
#include "esp_log.h"

#define TAG "PLAYLIST"

PlayList::PlayList() {
	maxTrack = -1;
	activeTrack = -1;
}

void PlayList::readDir( const char *directory ) {

	DIR *d;
    struct dirent *dir;
    char *ext1;
    char ext[10];
    audio_filetype_t ft;
    char *temp;
    int8_t i;

    d = opendir( directory );
    if (!d) return;

    while ((dir = readdir(d)) != NULL) {

    	// filter apple stuff
    	if ( dir->d_name[0] == '.' ) {
    			continue;
    	}

        // check if file has a valid extention
    	ext1 = strrchr( dir->d_name, '.' );
        ft = FILETYPE_UNKOWN;

        if ( ( ext1 != NULL ) && (strlen( ext1 ) < 10 ) ) {
        	// . found, get extention
        	strcpy( ext, strlwr( ext1 ) );

        	// check on valid extentions

        	if ( strcmp( ext, ".mp3") == 0 )  { ft = FILETYPE_MP3; }

        	// ogg has problems playing a song twice
        	// else if ( strcmp( ext, ".ogg") == 0 )  { ft = FILETYPE_OGG; }

        	else if ( strcmp( ext, ".wav") == 0 )  { ft = FILETYPE_WAV; }
        }

        if ( ft != FILETYPE_UNKOWN ) {

        	// add new entry
        	maxTrack++;
        	track[maxTrack]    = (char *)malloc( strlen(dir->d_name) + 1 );
        	filetype[maxTrack] = ft;
        	strcpy( track[maxTrack], dir->d_name );

        	// bubble sort
        	i=maxTrack;
        	while (i>0) {
        		// check if swap needed
        		if ( strcasecmp( track[i], track[i-1] ) < 0 ) {
        			temp          = track[i-1];
        			track[i-1]    = track[i];
        			track[i]      = temp;
        			ft            = filetype[i-1];
        			filetype[i-1] = filetype[i];
        			filetype[i]   = ft;
        			i--;
        		} else {
        			break;
        		}
        	}

        }

    }

    closedir(d);

    if (maxTrack >= 0) {
    	activeTrack = 0;
    }

    for (int i=0; i<=maxTrack; i++) {
    	ESP_LOGD(TAG, "track %d=%s", i, track[i]);
    }

}

char *PlayList::getTrack( int8_t trackNr ) {

	if ( ( trackNr > maxTrack ) || ( trackNr < 0 ) ) {
		return "none";
	} else {
		return track[trackNr];
	}
}

char *PlayList::getActiveTrack( void ) {

	return getTrack( activeTrack );

}

void PlayList::setActiveTrackNr( int8_t trackNr ) {

	if ( ( trackNr >=0 ) && ( trackNr <= maxTrack ) ) {
		activeTrack = trackNr;
	}

}


audio_filetype_t PlayList::getFiletype(int8_t trackNr) {

	if ( ( trackNr > maxTrack ) || ( trackNr < 0 ) ) {
		return FILETYPE_UNKOWN;
	} else {
		return filetype[trackNr];
	}
}


audio_filetype_t PlayList::getActiveFiletype(void) {

	return getFiletype( activeTrack );

}

int8_t PlayList::getTracks( void ) {

	return maxTrack + 1;

}

int8_t PlayList::getActiveTrackNr(void) {

	return activeTrack;

}

void PlayList::nextTrack( void ) {

	if ( activeTrack >= maxTrack ) {
		activeTrack = 0;
	} else {
		activeTrack++;
	}

}

void PlayList::prevTrack( void ) {

	if ( activeTrack <= 0 ) {
		activeTrack = maxTrack;
	} else {
		activeTrack--;
	}

}


void PlayList::setRandomTrack() {

	activeTrack = rand() % ( maxTrack + 1 );

}
