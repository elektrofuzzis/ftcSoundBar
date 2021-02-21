/*
 * pipeline.cpp
 *
 *  Created on: 30.12.2020
 *      Author: Stefan Fuss
 */

#include <esp_log.h>
#include <mp3_decoder.h>
#include <wav_decoder.h>
#include <ogg_decoder.h>

#include <audio_pipeline.h>
#include <audio_hal.h>
#include <string.h>

#include "playlist.h"
#include "pipeline.h"
#include "adfcorrections.h"

#define TAGPIPELINE "::PIPELINE"

Pipeline::Pipeline() {
	board_handle = NULL;
	pipeline = NULL;
	i2s_stream_writer = NULL;
	decoder_mp3 = NULL;
	decoder_wav = NULL;
	decoder_ogg = NULL;
	decoder = NULL;
	fatfs_stream_reader = NULL;
	decoder_filetype = FILETYPE_UNKOWN;
	mode = MODE_SINGLE_TRACK;
}

void Pipeline::StartCodec(void) {

	board_handle = audio_board_init();
	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

	ESP_LOGD(TAGPIPELINE, "Create audio pipeline for playback");
	audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	pipeline = audio_pipeline_init(&pipeline_cfg);
	mem_assert(pipeline);

	ESP_LOGD(TAGPIPELINE, "Create fatfs stream to read data from sdcard");
	fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
	fatfs_cfg.type = AUDIO_STREAM_READER;
	fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);

	ESP_LOGD(TAGPIPELINE, "Create i2s stream to write data to codec chip");
	i2s_stream_cfg_t i2s_cfg = _I2S_STREAM_CFG_DEFAULT();
	i2s_cfg.type = AUDIO_STREAM_WRITER;
	i2s_stream_writer = i2s_stream_init(&i2s_cfg);

	ESP_LOGD(TAGPIPELINE, "Create mp3 decoder to decode mp3 file");
	mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
	decoder_mp3 = mp3_decoder_init(&mp3_cfg);

	ESP_LOGD(TAGPIPELINE, "Create wav decoder to decode wav file");
	wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
	decoder_wav = wav_decoder_init(&wav_cfg);

	ESP_LOGD(TAGPIPELINE, "Create ogg decoder to decode ogg file");
	ogg_decoder_cfg_t ogg_cfg = DEFAULT_OGG_DECODER_CONFIG();
	decoder_ogg = ogg_decoder_init(&ogg_cfg);

}

void Pipeline::build( audio_filetype_t filetype )
{

	// need to unregister old pipeline?
	if (decoder != NULL ) {
		// unregister_pipeline
		audio_pipeline_unregister(pipeline, fatfs_stream_reader);
		audio_pipeline_unregister(pipeline, decoder);
		audio_pipeline_unregister(pipeline, i2s_stream_writer);
		audio_pipeline_unlink( pipeline );
		decoder = NULL;
		decoder_filetype = FILETYPE_UNKOWN;
	}

	// choose new decoder
	switch (filetype) {
	case FILETYPE_MP3: decoder = decoder_mp3; break;
	case FILETYPE_WAV: decoder = decoder_wav; break;
	case FILETYPE_OGG: decoder = decoder_ogg; break;
	default: return; break;
	}

	// save new filetype
	decoder_filetype = filetype;

	// build new pipeline
	audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
	audio_pipeline_register(pipeline, decoder, "decoder");
	audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

	//audio_element_set_event_callback(decoder, audio_element_event_handler, NULL);
	//audio_element_set_event_callback(fatfs_stream_reader, audio_element_event_handler, NULL);
	//audio_element_set_event_callback(i2s_stream_writer, audio_element_event_handler, NULL);

	ESP_LOGD(TAGPIPELINE, "Link it together [sdcard]-->fatfs_stream-->decoder-->i2s_stream-->[codec_chip]");
	const char *link_tag[3] = {"file", "decoder", "i2s"};
	audio_pipeline_link(pipeline, &link_tag[0], 3);

}

void Pipeline::stop( void ) {

	ESP_LOGD( TAGPIPELINE, "stop_track");

	audio_pipeline_stop(pipeline);
	audio_pipeline_wait_for_stop(pipeline);
	audio_pipeline_terminate(pipeline);

}

void Pipeline::play( void ) {

	play( playList.getActiveTrack(), playList.getActiveFiletype() );
}


void Pipeline::play( char *url, audio_filetype_t filetype) {

	if (playList.getActiveTrackNr()<0) {
		ESP_LOGD( TAGPIPELINE, "PLAY: no valid track selected");
		return;
	}

	if ( filetype == FILETYPE_UNKOWN ) {
		ESP_LOGD( TAGPIPELINE, "PLAY: FILETYPE_UNKOWN");
		return;
	}

	ESP_LOGD( TAGPIPELINE, "PLAY: url=%s filetype=%d", url, filetype );

    // stop running track
    //if ( audio_element_get_state(i2s_stream_writer) == AEL_STATE_RUNNING) {
    	stop( );
    //}

	if (decoder_filetype != filetype ) {
		ESP_LOGD( TAGPIPELINE, "PLAY: relink ");
		build( filetype );

	}

    char *url2 = (char *) malloc( strlen( url ) + 10);
    sprintf( url2, "/sdcard/%s", url );

    esp_err_t err;

    // start track
    err = audio_element_set_uri( fatfs_stream_reader, url2 );
    if (err != ESP_OK) { ESP_LOGD( TAGPIPELINE, "PLAY: audio_element_set_uri: %s %d", url2, err ); }

    err = audio_pipeline_reset_ringbuffer( pipeline );
    if (err != ESP_OK) { ESP_LOGD( TAGPIPELINE, "PLAY: audio_pipeline_reset_ringbuffer: %d", err ); }

    err = audio_pipeline_reset_elements( pipeline );
    if (err != ESP_OK) { ESP_LOGD( TAGPIPELINE, "PLAY: audio_pipeline_reset_elements: %d", err ); }

    err = audio_pipeline_run( pipeline );
    if (err != ESP_OK) { ESP_LOGD( TAGPIPELINE, "PLAY: audio_pipeline_run: %d", err ); }

    free(url2);

}

void Pipeline::setMode( play_mode_t newMode ) {
	mode = newMode;
}

play_mode_t Pipeline::getMode( void ) {
	return mode;
}

esp_err_t Pipeline::resume(void) {
	return audio_pipeline_resume( pipeline );
}

esp_err_t Pipeline::pause(void) {
	return audio_pipeline_pause( pipeline );
}

bool Pipeline::isPlaying( void ) {
	return (audio_element_get_state( i2s_stream_writer ) == AEL_STATE_RUNNING);
}

int Pipeline::getVolume( void ) {

	int player_volume = 0;

	audio_hal_get_volume( board_handle->audio_hal, &player_volume) ;
	return player_volume;

}

void Pipeline::setVolume( int volume ) {

	int player_volume ;

	// get actual volume
	player_volume = getVolume();

	// change the value
	switch (volume) {
	case DEC_VOLUME: player_volume -= 10; break;
	case INC_VOLUME: player_volume += 10; break;
	default:         player_volume = volume; break;
	}

	// check boundaries
	if (player_volume > 100) {
	    player_volume = 100;
	} else if (player_volume < 0) {
	   	player_volume = 0;
	}

	// set new value
	audio_hal_set_volume( board_handle->audio_hal, player_volume );

}

audio_element_state_t Pipeline::getState( void ) {

	return audio_element_get_state( decoder );

}

void Pipeline::setListener( audio_event_iface_handle_t evt ) {

	audio_pipeline_set_listener( pipeline, evt );

}

audio_board_handle_t Pipeline::getBoardHandle( void ) {

	return board_handle;

}

void Pipeline::audioMessageHandler( audio_event_iface_msg_t msg ) {

   	// check on next song

   	if ( ( msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT) &&
   	     ( msg.source == (void *)i2s_stream_writer ) ) {

   		if ( ( msg.cmd == AEL_MSG_CMD_REPORT_STATUS ) &&
	         ( getState() == AEL_STATE_FINISHED ) ) {

   			switch ((int) mode ) {
   			case MODE_SHUFFLE:
   				playList.setRandomTrack();
   				ESP_LOGI(TAGPIPELINE, "SHUFFLE next track=%d", playList.getActiveTrackNr() );
   				play( playList.getActiveTrack(), playList.getActiveFiletype() );
   				break;
   			case MODE_REPEAT:
   				ESP_LOGI(TAGPIPELINE, "REPEAT");
   				play( playList.getActiveTrack(), playList.getActiveFiletype() );
   				break;
   			default:
   				break;
   			}
   		}
   	}

}
