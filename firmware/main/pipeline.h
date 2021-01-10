/*
 * pipeline.h
 *
 *  Created on: 30.12.2020
 *      Author: Stefan Fuss
 */

#ifndef MAIN_PIPELINE_H_
#define MAIN_PIPELINE_H_

#include <audio_pipeline.h>
#include <fatfs_stream.h>
#include <i2s_stream.h>
#include <board.h>

#include "playlist.h"

typedef enum {
	DEC_VOLUME = -2,
	INC_VOLUME = -1
} volume_t;

typedef enum {
	MODE_SINGLE_TRACK = 0,
	MODE_SHUFFLE = 1,
	MODE_REPEAT = 2
} play_mode_t;

class Pipeline {
private:
	audio_board_handle_t board_handle;
	audio_pipeline_handle_t pipeline;
	audio_element_handle_t i2s_stream_writer;
	audio_element_handle_t decoder_mp3, decoder_wav, decoder_ogg, decoder;
	audio_element_handle_t fatfs_stream_reader;
	audio_filetype_t decoder_filetype;
	play_mode_t mode;
public:
	PlayList playList;
	Pipeline();
	void StartCodec(void);
	void stop( void );
	void play( void );
	void play( char *url, audio_filetype_t filetype);
	void build( audio_filetype_t filetype );
	void setMode( play_mode_t newMode );
	play_mode_t getMode( void );
	esp_err_t resume( void );
	esp_err_t pause( void );
	bool isPlaying( void );
	int getVolume( void );
	void setVolume( int volume );
	audio_element_state_t getState( void );
	void setListener( audio_event_iface_handle_t evt );
	audio_board_handle_t getBoardHandle( void );
	void eventLoop( void );
};

#endif /* MAIN_PIPELINE_H_ */
