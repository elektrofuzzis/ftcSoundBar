/*
 * playlist.h
 *
 *  Created on: 09.12.2020
 *      Author: Stefan Fuss
 */

#ifndef MAIN_PLAYLIST_H_
#define MAIN_PLAYLIST_H_

#include <stdint.h>

#define MAXTRACK 100

typedef enum {
	FILETYPE_UNKOWN,
	FILETYPE_AAC,
	FILETYPE_MP3,
	FILETYPE_OGG,
	FILETYPE_WAV
} audio_filetype_t;

class PlayList {
private:
	int8_t maxTrack;
	int8_t activeTrack;
	char   *track[MAXTRACK];
	audio_filetype_t filetype[MAXTRACK];
public:
	PlayList();
	void readDir( const char *directory );
	char *getTrack( int8_t trackNr );
	char *getActiveTrack( void );
	void setActiveTrackNr( int8_t trackNr );
	int8_t getActiveTrackNr(void);
	audio_filetype_t getActiveFiletype(void);
	audio_filetype_t getFiletype(int8_t trackNr);
	int8_t getTracks( void );
	void nextTrack( void );
	void prevTrack( void );
	void setRandomTrack( void );
};

#endif /* MAIN_PLAYLIST_H_ */
