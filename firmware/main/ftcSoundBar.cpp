
/****************************************************************************************
 *
 * ftcSoundBar - a LyraT based fischertechnik compatible music box.
 *
 * Version 0.1
 *
 * (C) 2020 Oliver Schmied, Christian Bergschneider & Stefan Fuss
 *
 * Please run idf.py menuconfig, first
 *
 * EXAMPLE-CONFIGURATION: setting SSID & Password is helpful, but not mandatory
 * COMPONENT-CONFIGURATION/FAT FILESYSTEM SUPPORT: long filename support, UTF 8 encoding
 *
 ****************************************************************************************/

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <audio_pipeline.h>
#include <fatfs_stream.h>
#include <i2s_stream.h>

// only the working decoders
#include <aac_decoder.h>
#include <mp3_decoder.h>
#include <ogg_decoder.h>
#include <wav_decoder.h>

#include <filter_resample.h>
#include <esp_flash_partitions.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <input_key_service.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_vfs.h>
#include <esp_http_server.h>
#include <cJSON.h>
#include <stdbool.h>
#include <mdns.h>
#include "adfcorrections.h"
#include "playlist.h"

extern "C" {
    void app_main(void);
}


#define FIRMWARE_VERSION "v1.0b"

#define CONFIG_FILE "/sdcard/ftcSoundBar.conf"
static const char *TAG = "ftcSoundBar";
#define FIRMWAREUPDATE "/sdcard/ftcSoundBar.bin"

// some ftcSoundBar definitions
typedef enum {
	DEC_VOLUME = -2,
	INC_VOLUME = -1
} volume_t;

typedef enum {
	MODE_SINGLE_TRACK = 0,
	MODE_SHUFFLE = 1,
	MODE_REPEAT = 2
} play_mode_t;

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

#define BUFFSIZE 1024
static char ota_write_data[BUFFSIZE + 1] = { 0 };

#define BLINK_GPIO 22

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct http_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} http_server_context_t;


esp_err_t audio_element_event_handler(audio_element_handle_t self, audio_event_iface_msg_t *event, void *ctx);

#define TAGPIPELINE "::PIPELINE"

class Pipeline {
public:
	audio_board_handle_t board_handle;
	audio_pipeline_handle_t pipeline;
	audio_element_handle_t i2s_stream_writer, decoder, fatfs_stream_reader;
	audio_filetype_t decoder_filetype;
	Pipeline();
	void StartCodec(void);
	void stop( void );
	void play( char *url, audio_filetype_t filetype);
	void build( audio_filetype_t filetype );
	esp_err_t event_handler(audio_element_handle_t self, audio_event_iface_msg_t *event, void *ctx);
};

Pipeline::Pipeline() {
	board_handle = NULL;
	pipeline = NULL;
	i2s_stream_writer = NULL;
	decoder = NULL;
	fatfs_stream_reader = NULL;
	decoder_filetype = FILETYPE_UNKOWN;
}

void Pipeline::StartCodec(void) {

	board_handle = audio_board_init();
	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

	ESP_LOGI(TAGPIPELINE, "Create audio pipeline for playback");
	audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	pipeline = audio_pipeline_init(&pipeline_cfg);
	mem_assert(pipeline);

	ESP_LOGI(TAGPIPELINE, "Create i2s stream to write data to codec chip");
	i2s_stream_cfg_t i2s_cfg = _I2S_STREAM_CFG_DEFAULT();
	i2s_cfg.type = AUDIO_STREAM_WRITER;
	i2s_stream_writer = i2s_stream_init(&i2s_cfg);
	audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

	ESP_LOGI(TAGPIPELINE, "Create fatfs stream to read data from sdcard");
	fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
	fatfs_cfg.type = AUDIO_STREAM_READER;
	fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);
	audio_pipeline_register(pipeline, fatfs_stream_reader, "file");

}

void Pipeline::build( audio_filetype_t filetype ) {

	// drop old decoder
	if ( (decoder != NULL) && (decoder_filetype != filetype) ) {
		ESP_LOGI(TAGPIPELINE, "unregister decoder");
		audio_pipeline_unregister(pipeline, decoder);
		audio_element_deinit(decoder);
	}

	switch (filetype) {
	case FILETYPE_AAC: {
			ESP_LOGI(TAGPIPELINE, "Create aac decoder to decode aac file");
			aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
			decoder = aac_decoder_init(&aac_cfg);
			decoder_filetype = FILETYPE_AAC;
			break; }
	case FILETYPE_MP3: {
		ESP_LOGI(TAGPIPELINE, "Create mp3 decoder to decode mp3 file");
		mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
		decoder = mp3_decoder_init(&mp3_cfg);
		decoder_filetype = FILETYPE_MP3;
		break; }
	case FILETYPE_OGG: {
		ESP_LOGI(TAGPIPELINE, "Create ogg decoder to decode ogg file");
		ogg_decoder_cfg_t ogg_cfg = DEFAULT_OGG_DECODER_CONFIG();
		decoder = ogg_decoder_init(&ogg_cfg);
		decoder_filetype = FILETYPE_OGG;
		break; }
	case FILETYPE_WAV: {
		ESP_LOGI(TAGPIPELINE, "Create wav decoder to decode wav file");
		wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
		decoder = wav_decoder_init(&wav_cfg);
		decoder_filetype = FILETYPE_WAV;
		break; }
    default:
    	filetype = FILETYPE_UNKOWN;
		break;
	}

	audio_element_set_event_callback(decoder, audio_element_event_handler, NULL);
	audio_element_set_event_callback(fatfs_stream_reader, audio_element_event_handler, NULL);

	ESP_LOGI(TAGPIPELINE, "Register all elements to audio pipeline");

	// register new decoder
	audio_pipeline_register(pipeline, decoder, "decoder");

	ESP_LOGI(TAGPIPELINE, "Link it together [sdcard]-->fatfs_stream-->decoder-->i2s_stream-->[codec_chip]");
	const char *link_tag[3] = {"file", "decoder", "i2s"};
	audio_pipeline_link(pipeline, &link_tag[0], 3);
}

void Pipeline::stop( void ) {

	ESP_LOGI( TAGPIPELINE, "stop_track");

	ESP_LOGI( TAGPIPELINE, "stop %d", audio_element_get_state(i2s_stream_writer) == AEL_STATE_RUNNING );
	audio_pipeline_stop(pipeline);
	ESP_LOGI( TAGPIPELINE, "wait %d", audio_element_get_state(i2s_stream_writer) == AEL_STATE_RUNNING );
	audio_pipeline_wait_for_stop(pipeline);
	ESP_LOGI( TAGPIPELINE, "terminate %d", audio_element_get_state(i2s_stream_writer) == AEL_STATE_RUNNING );
	audio_pipeline_terminate(pipeline);
	ESP_LOGI( TAGPIPELINE, "done %d", audio_element_get_state(i2s_stream_writer) == AEL_STATE_RUNNING );
}

void Pipeline::play( char *url, audio_filetype_t filetype) {

	if ( filetype == FILETYPE_UNKOWN ) {
		return;
	}

	ESP_LOGI( TAGPIPELINE, "play");

    // stop running track
    if ( audio_element_get_state(i2s_stream_writer) == AEL_STATE_RUNNING) {
    	stop( );
    }

	if ( decoder_filetype != filetype ) {
		ESP_LOGI( TAGPIPELINE, "relink ");

		//audio_pipeline_deinit( pipeline);
		build( filetype );

	}

    char *url2 = (char *)malloc( strlen( url ) + 10);
    sprintf( url2, "/sdcard/%s", url );

    esp_err_t err;

    // start track
    err = audio_element_set_uri( fatfs_stream_reader, url2 );
    if (err != ESP_OK) { ESP_LOGI( TAGPIPELINE, "audio_element_set_uri: %d", err ); }

    err = audio_pipeline_reset_ringbuffer( pipeline );
    if (err != ESP_OK) { ESP_LOGI( TAGPIPELINE, "audio_pipeline_reset_ringbuffer: %d", err ); }

    err = audio_pipeline_reset_elements( pipeline );
    if (err != ESP_OK) { ESP_LOGI( TAGPIPELINE, "audio_pipeline_reset_elements: %d", err ); }

    err = audio_pipeline_run( pipeline );
    if (err != ESP_OK) { ESP_LOGI( TAGPIPELINE, "audio_pipeline_run: %d", err ); }

    free(url2);

}

esp_err_t Pipeline::event_handler(audio_element_handle_t self, audio_event_iface_msg_t *event, void *ctx)
{
    ESP_LOGI(TAGPIPELINE, "Audio event %d from %s element", event->cmd, audio_element_get_tag(self));

    if (event->cmd == AEL_MSG_CMD_REPORT_STATUS) {
        switch ((int) event->data) {
            case AEL_STATUS_STATE_RUNNING:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_STATE_RUNNING");
                break;
            case AEL_STATUS_STATE_STOPPED:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_STATE_STOPPED");
                break;
            case AEL_STATUS_STATE_PAUSED:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_STATE_PAUSED");
                break;
            case AEL_STATUS_STATE_FINISHED:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_STATE_FINISHED");
                break;
            case AEL_STATUS_INPUT_DONE:
            	ESP_LOGI(TAGPIPELINE, "AEL_STATUS_INPUT_DONE");
            	break;
            case AEL_STATUS_INPUT_BUFFERING:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_INPUT_BUFFERING");
                break;
            case AEL_STATUS_OUTPUT_DONE:
            	ESP_LOGI(TAGPIPELINE, "AEL_STATUS_OUTPUT_DONE");
            	break;
            case AEL_STATUS_OUTPUT_BUFFERING:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_OUTPUT_BUFFERING");
                break;
            case AEL_STATUS_ERROR_OPEN:
            	ESP_LOGI(TAGPIPELINE, "AEL_STATUS_ERROR_OPEN");
            	break;
            case AEL_STATUS_ERROR_INPUT:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_ERROR_INPUT");
                break;
            case AEL_STATUS_ERROR_PROCESS:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_ERROR_PROCESS");
                break;
            case AEL_STATUS_ERROR_OUTPUT:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_ERROR_OUTPUT");
                break;
            case AEL_STATUS_ERROR_CLOSE:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_ERROR_CLOSE");
                break;
            case AEL_STATUS_ERROR_TIMEOUT:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_ERROR_TIMEOUT");
                break;
            case AEL_STATUS_MOUNTED:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_MOUNTED");
                break;
            case AEL_STATUS_UNMOUNTED:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_UNMOUNTED");
                break;
            case AEL_STATUS_ERROR_UNKNOWN:
                ESP_LOGI(TAGPIPELINE, "AEL_STATUS_ERROR_UNKNOWN");
                break;
            default:
                ESP_LOGI(TAGPIPELINE, "Some other event = %d", (int) event->data);
        }
    }
    return ESP_OK;
}

typedef struct {

	Pipeline pipeline;
	PlayList playList;
	char WIFI_SSID[32];
	char WIFI_PASSWORD[64];
	uint8_t TXT_AP_MODE;
	char HOSTNAME[64];

	uint16_t I2C_ADDRESS;
	int mode;
	TaskHandle_t xBlinky;

} ftcSoundBar_t;

ftcSoundBar_t ftcSoundBar;

void ftcSoundBar_init( ftcSoundBar_t *ftcSoundBar ) {

	ftcSoundBar->mode = MODE_SINGLE_TRACK;
	ftcSoundBar->xBlinky = NULL;

	strcpy( ftcSoundBar->WIFI_SSID, "YOUR_SSID");
	strcpy( ftcSoundBar->WIFI_PASSWORD, "YOUR_PASSWORD");

	ftcSoundBar->TXT_AP_MODE = 0;
	strcpy( ftcSoundBar->HOSTNAME, "ftcSoundBar" );
	ftcSoundBar->I2C_ADDRESS = 64;

}

esp_err_t audio_element_event_handler(audio_element_handle_t self, audio_event_iface_msg_t *event, void *ctx)
{
	return ftcSoundBar.pipeline.event_handler(self, event, ctx);
}


/**************************************************************************************
 *
 * basic sd card functions
 *
 **************************************************************************************/

#define CONFTAG "CONFIG"

void write_config_file( void )
{
	FILE* f;

   	f = fopen(CONFIG_FILE, "w");

    if (f == NULL) {
    	ESP_LOGE(CONFTAG, "Could not write %s.", CONFIG_FILE);
    	return;
    }

    fprintf( f, "WIFI_SSID=%s\n", ftcSoundBar.WIFI_SSID);
    fprintf( f, "WIFI_PASSWORD=%s\n", ftcSoundBar.WIFI_PASSWORD);
    fprintf( f, "TXT_AP_MODE=%d\n", ftcSoundBar.TXT_AP_MODE);
    fprintf( f, "I2C_ADDRESS=%d\n", ftcSoundBar.I2C_ADDRESS);
    fprintf( f, "HOSTNAME=%s\n", ftcSoundBar.HOSTNAME);

    fclose(f);

}

void read_config_file( void )
{   FILE* f;

    struct stat st;
    if (stat(CONFIG_FILE, &st) == 0) {
    	// CONFIG_FILE exists, start reading it
    	f = fopen(CONFIG_FILE, "r");

    	if (f == NULL) {
    		ESP_LOGE(CONFTAG, "Could not read %s.", CONFIG_FILE);
    	    return;
    	}

    	char line[256];
    	char key[256];
    	char value[256];

    	while ( fscanf( f, "%s\n", line ) > 0 ) {
    		strcpy( key, strtok( line, "=" ) );
    		strcpy( value, strtok( NULL, "=" ) );

    		if ( strcmp( key, "WIFI_SSID" ) == 0 ) {

    			memcpy( ftcSoundBar.WIFI_SSID, value, 32 );

    		} else if ( strcmp( key, "WIFI_PASSWORD" ) == 0 ) {

    			memcpy( ftcSoundBar.WIFI_PASSWORD, value, 64 );

    		} else if ( strcmp( key, "TXT_AP_MODE" ) == 0 ) {

    		    ftcSoundBar.TXT_AP_MODE = ( atoi( value ) != 0 );

    		} else if ( strcmp( key, "I2C_ADDRESS" ) == 0 ) {

    			ftcSoundBar.I2C_ADDRESS = atoi( value );

    		} else if ( strcmp( key, "HOSTNAME" ) == 0 ) {

    			strcpy( ftcSoundBar.HOSTNAME, value );

    		} else {

    			ESP_LOGW(CONFTAG, "reading config file, ignoring pair (%s=%s)\n", key, value);
    		}

    	}

        fclose(f);

    } else {

    	// CONFIG_FILE is missing, create a sample file
    	ESP_LOGE(CONFTAG, "%s not found. Creating empty config file. Please set your parameters!", CONFIG_FILE);

    	write_config_file();

    }

}

/**************************************************************************************
 *
 * simple tasks
 *
 **************************************************************************************/


void task_reboot(void *pvParameter)
{
	ESP_LOGI( TAG, "I will reboot in 0.5s...");
	vTaskDelay(500 / portTICK_RATE_MS);
	esp_restart();

}

void task_blinky(void *pvParameter)
{

    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction((gpio_num_t)BLINK_GPIO, GPIO_MODE_OUTPUT);
    while( 1 ) {
        /* Blink off (output low) */
        gpio_set_level((gpio_num_t)BLINK_GPIO, 0);
        vTaskDelay(250 / portTICK_RATE_MS);
        /* Blink on (output high) */
        gpio_set_level((gpio_num_t)BLINK_GPIO, 1);
        vTaskDelay(250 / portTICK_RATE_MS);
    }

}

#define OTATAG "OTA"

static void __attribute__((noreturn)) task_fatal_error()
{
    ESP_LOGE(OTATAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

void task_ota(void *pvParameter)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(OTATAG, "Starting OTA...");

    xTaskCreate(&task_blinky, "blinky", 512, NULL, 5, &(ftcSoundBar.xBlinky) );

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(OTATAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(OTATAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(OTATAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    FILE *f;
    f = fopen(FIRMWAREUPDATE, "rb");
    if ( f == NULL ) {
    	ESP_LOGI( TAG, "ftcSoundBar.bin not found");
    	task_fatal_error();
    }

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(OTATAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    int binary_file_length = 0;
     /*deal with all receive packet*/
     bool image_header_was_checked = false;
     while (1) {
    	 //int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
         int data_read = fread( ota_write_data, 1, BUFFSIZE, f );
         ESP_LOGI( OTATAG, "data_read=%d", data_read);
         if (data_read < 0) {
             ESP_LOGE(OTATAG, "Error: file data read error");
             task_fatal_error();
         } else if (data_read > 0) {
             if (image_header_was_checked == false) {
                 esp_app_desc_t new_app_info;
                 if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                     // check current version with downloading
                     memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                     ESP_LOGI(OTATAG, "New firmware version: %s", new_app_info.version);

                     esp_app_desc_t running_app_info;
                     if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                         ESP_LOGI(OTATAG, "Running firmware version: %s", running_app_info.version);
                     }

                     const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
                     esp_app_desc_t invalid_app_info;
                     if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
                         ESP_LOGI(OTATAG, "Last invalid firmware version: %s", invalid_app_info.version);
                     }

                     // check current version with last invalid partition
                     if (last_invalid_app != NULL) {
                         if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
                             ESP_LOGW(OTATAG, "New version is the same as invalid version.");
                             ESP_LOGW(OTATAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                             ESP_LOGW(OTATAG, "The firmware has been rolled back to the previous version.");
                             task_fatal_error();
                         }
                     }

                     image_header_was_checked = true;

                     err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                     if (err != ESP_OK) {
                         ESP_LOGE(OTATAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                         task_fatal_error();
                     }
                     ESP_LOGI(OTATAG, "esp_ota_begin succeeded");
                 } else {
                     ESP_LOGE(OTATAG, "received package is not fit len");
                     task_fatal_error();
                 }
             }
             err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
             if (err != ESP_OK) {
                 task_fatal_error();
             }
             binary_file_length += data_read;
             ESP_LOGD(OTATAG, "Written image length %d", binary_file_length);
         } else if (data_read == 0) {

                 ESP_LOGI(OTATAG, "EOF");
                 break;

        }
     }
     ESP_LOGI(OTATAG, "Total Write binary data length: %d", binary_file_length);

     fclose(f);

     err = esp_ota_end(update_handle);
     if (err != ESP_OK) {
         if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
             ESP_LOGE(OTATAG, "Image validation failed, image is corrupted");
         }
         ESP_LOGE(OTATAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
         task_fatal_error();
     }

     err = esp_ota_set_boot_partition(update_partition);
     if (err != ESP_OK) {
         ESP_LOGE(OTATAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
         task_fatal_error();
     }
     ESP_LOGI(OTATAG, "Prepare to restart system!");

    esp_restart();

}

/**************************************************************************************
 *
 * basic API funktions
 *
 **************************************************************************************/

/**
 * @brief change volumne
 *
 * @param vol DEC_VOLUME, INC_VOLUME or an absolute value between 0 and 100
 *
 * @return
 */
void volume( int vol) {

	int player_volume ;

	// get actual volume
	audio_hal_get_volume(ftcSoundBar.pipeline.board_handle->audio_hal, &player_volume);

	// change the value
	switch (vol) {
	  case DEC_VOLUME: player_volume -= 10; break;
	  case INC_VOLUME: player_volume += 10; break;
	  default:         player_volume = vol; break;
	}

	// check boundaries
	if (player_volume > 100) {
        player_volume = 100;
    } else if (player_volume < 0) {
    	player_volume = 0;
    }

	// set new value
	audio_hal_set_volume(ftcSoundBar.pipeline.board_handle->audio_hal, player_volume);

}

/******************************************************************
 *
 * WebServer
 *
 ******************************************************************/

/**
 * @brief send header of web site
 *
 * @param req
 *
 * @return error-code
 */
static esp_err_t send_header(httpd_req_t *req )
{
	extern const unsigned char header_start[] asm("_binary_header_html_start");
    extern const unsigned char header_end[]   asm("_binary_header_html_end");
    const size_t header_size = (header_end - header_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, (const char *)header_start, header_size);
    return ESP_OK;
}

/**
 * @brief send footer of web site
 *
 * @param req
 *
 * @return error-code
 */
static esp_err_t send_footer(httpd_req_t *req )
{
	extern const unsigned char footer_start[] asm("_binary_footer_html_start");
    extern const unsigned char footer_end[]   asm("_binary_footer_html_end");
    const size_t footer_size = (footer_end - footer_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, (const char *)footer_start, footer_size);
    return ESP_OK;
}

/**
 * @brief send favicon.ico
 *
 * @param req
 *
 * @return error-code
 */
static esp_err_t send_img(httpd_req_t *req )
{
	extern const unsigned char ftcSound_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char ftcSound_end[]   asm("_binary_favicon_ico_end");
    const size_t ftcSound_size = (ftcSound_end - ftcSound_start);

    httpd_resp_set_type(req, "image/ico");
    httpd_resp_send_chunk(req, (const char *)ftcSound_start, ftcSound_size);
    return ESP_OK;
}

/**
 * @brief deliver favicon.ico
 *
 * @param req
 *
 * @return error-code
 */
static esp_err_t favico_get_handler(httpd_req_t *req )
{
	send_img(req);
    httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

static esp_err_t img_get_handler(httpd_req_t *req )
{

	if ( strcmp( req->uri, "/img/ftcsoundbarlogo.png") == 0 ) {

    	extern const unsigned char ftcSoundbar_start[] asm("_binary_ftcsoundbarlogo_png_start");
		extern const unsigned char ftcSoundbar_end[]   asm("_binary_ftcsoundbarlogo_png_end");
		const size_t ftcSoundbar_size = (ftcSoundbar_end - ftcSoundbar_start);
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send_chunk(req, (const char *)ftcSoundbar_start, ftcSoundbar_size);

	} else if ( strcmp( req->uri, "/img/cocktail.svg") == 0 ) {

		extern const unsigned char cocktail_start[] asm("_binary_cocktail_svg_start");
		extern const unsigned char cocktail_end[]   asm("_binary_cocktail_svg_end");
		const size_t cocktail_size = (cocktail_end - cocktail_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)cocktail_start, cocktail_size);

	} else if ( strcmp( req->uri, "/img/next.svg") == 0 ) {

		extern const unsigned char next_start[] asm("_binary_next_svg_start");
		extern const unsigned char next_end[]   asm("_binary_next_svg_end");
		const size_t next_size = (next_end - next_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)next_start, next_size);

	} else if ( strcmp( req->uri, "/img/play.svg") == 0 ) {

		extern const unsigned char play_start[] asm("_binary_play_svg_start");
		extern const unsigned char play_end[]   asm("_binary_play_svg_end");
		const size_t play_size = (play_end - play_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)play_start, play_size);

	} else if ( strcmp( req->uri, "/img/previous.svg") == 0 ) {

		extern const unsigned char previous_start[] asm("_binary_previous_svg_start");
		extern const unsigned char previous_end[]   asm("_binary_previous_svg_end");
		const size_t previous_size = (previous_end - previous_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)previous_start, previous_size);

	} else if ( strcmp( req->uri, "/img/stop.svg") == 0 ) {

		extern const unsigned char stop_start[] asm("_binary_stop_svg_start");
		extern const unsigned char stop_end[]   asm("_binary_stop_svg_end");
		const size_t stop_size = (stop_end - stop_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)stop_start, stop_size);

	} else if ( strcmp( req->uri, "/img/volumeup.svg") == 0 ) {

		extern const unsigned char volumeup_start[] asm("_binary_volumeup_svg_start");
		extern const unsigned char volumeup_end[]   asm("_binary_volumeup_svg_end");
		const size_t volumeup_size = (volumeup_end - volumeup_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)volumeup_start, volumeup_size);

	} else if ( strcmp( req->uri, "/img/volumedown.svg") == 0 ) {

		extern const unsigned char volumedown_start[] asm("_binary_volumedown_svg_start");
		extern const unsigned char volumedown_end[]   asm("_binary_volumedown_svg_end");
		const size_t volumedown_size = (volumedown_end - volumedown_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)volumedown_start, volumedown_size);

	} else if ( strcmp( req->uri, "/img/random.svg") == 0 ) {

		extern const unsigned char random_start[] asm("_binary_random_svg_start");
		extern const unsigned char random_end[]   asm("_binary_random_svg_end");
		const size_t random_size = (random_end - random_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)random_start, random_size);

	} else if ( strcmp( req->uri, "/img/redo.svg") == 0 ) {

		extern const unsigned char redo_start[] asm("_binary_redo_svg_start");
		extern const unsigned char redo_end[]   asm("_binary_redo_svg_end");
		const size_t redo_size = (redo_end - redo_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)redo_start, redo_size);

	} else if ( strcmp( req->uri, "/img/cog.svg") == 0 ) {

		extern const unsigned char cog_start[] asm("_binary_cog_svg_start");
		extern const unsigned char cog_end[]   asm("_binary_cog_svg_end");
		const size_t cog_size = (cog_end - cog_start);
		httpd_resp_set_type(req, "image/svg+xml");
		httpd_resp_send_chunk(req, (const char *)cog_start, cog_size);

	}

    httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

/**
 * @brief deliver styles.css
 *
 * @param req
 *
 * @return error-code
 */
static esp_err_t styles_css_get_handler(httpd_req_t *req )
{
	extern const unsigned char styles_start[] asm("_binary_styles_css_start");
    extern const unsigned char styles_end[]   asm("_binary_styles_css_end");
    const size_t styles_size = (styles_end - styles_start);

    httpd_resp_set_type(req, "text/css");
    httpd_resp_send_chunk(req, (const char *)styles_start, styles_size);

	/* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

/**
 * @brief deliver iframe active_track
 *
 * @param req
 *
 * @return error-code
 */

static esp_err_t active_track_html_get_handler(httpd_req_t *req ) {

	char line[256];

	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html>" );
	httpd_resp_sendstr_chunk(req, "<html lang=\"de\" xml:lang=\"de\" xmlns=\"http://www.w3.org/1999/xhtml\">" );
	httpd_resp_sendstr_chunk(req, "<link rel=\"stylesheet\" href=\"styles.css\">" );
	httpd_resp_sendstr_chunk(req, "<head>" );
	httpd_resp_sendstr_chunk(req, "<meta name=\"encoding\" content=\"utf-8\">" );
	httpd_resp_sendstr_chunk(req, "<meta http-equiv=\"refresh\" content=\"3\">" );
	httpd_resp_sendstr_chunk(req, "</head>" );
	httpd_resp_sendstr_chunk(req, "<body>" );

	// ACTIVE TRACK
	if ( ftcSoundBar.playList.getActiveTrackNr() >= 0 ) {
       sprintf( line, "<p align=\"center\">%03d %s</p>", ftcSoundBar.playList.getActiveTrackNr()+1, ftcSoundBar.playList.getActiveTrack() );
       httpd_resp_sendstr_chunk(req, line);
    } else {
       httpd_resp_sendstr_chunk(req, "<p align=\"center\">none</p>");
    }

	httpd_resp_sendstr_chunk(req, "</body>" );
	httpd_resp_sendstr_chunk(req, "</html>" );

	// Send empty chunk to signal HTTP response completion
    httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;

}

/**
 * @brief deliver html site setup
 *
 * @param req
 *
 * @return error-code
 */
static esp_err_t setup_html_get_handler(httpd_req_t *req )
{
	char line[256];

	send_header(req);

	httpd_resp_sendstr_chunk(req, "<table align=\"center\">" );

	sprintf(line, "<tr><td style=\"text-align:right\">firmware version:</td><td style=\"text-align:left\">%s</td></tr>", FIRMWARE_VERSION );
	httpd_resp_sendstr_chunk(req, line );

	sprintf(line, "<tr><td style=\"text-align:right\">wifi SSID:</td><td><input type=\"text\" value=\"%s\" id=\"wifi_ssid\"></td></tr>", ftcSoundBar.WIFI_SSID );
	httpd_resp_sendstr_chunk(req, line );

	sprintf(line, "<tr><td style=\"text-align:right\">pre-shared key:</td><td><input type=\"password\" value=\"%s\" id=\"wifi_password\"></td></tr>", ftcSoundBar.WIFI_PASSWORD );
	httpd_resp_sendstr_chunk(req, line );

	char checked[20] = "";
	char display[30] = "";

	if ( ftcSoundBar.TXT_AP_MODE) {
		strcpy( checked, "checked");
	} else {
		strcpy( display, "style=\"display:none\"");
	}

	sprintf( line, "<tr><td style=\"text-align:right\">txt client mode:</td><td style=\"text-align:left\"><label class=\"switch\"><input type=\"checkbox\" id=\"txt_ap_mode\" %s><span class=\"slider round\"></span></label></td></tr>", checked);
	httpd_resp_sendstr_chunk(req, line );

	// sprintf( "<div id=\"ip\" %s><tr><td style=\"text-align:right\">ip address:</td><td style=\"text-align:left\">192.168.8.100</td></tr><div>", display);
	// httpd_resp_sendstr_chunk(req, line );

	if ( access( FIRMWAREUPDATE, F_OK ) != -1 ) {
		sprintf( line, "<tr><td><button type=\"button\" onclick=\"ota()\">update firmware</button></td><td><button type=\"button\" onclick=\"save_config('%s')\">save configuration</button></td></tr>", ftcSoundBar.HOSTNAME);
		httpd_resp_sendstr_chunk(req, line );
	} else {
		sprintf( line, "<tr><td colspan=\"2\"><button type=\"button\" onclick=\"save_config('%s')\">save configuration</button></td></tr>", ftcSoundBar.HOSTNAME);
		httpd_resp_sendstr_chunk(req, line );
	}

	httpd_resp_sendstr_chunk(req, "</table><hr>" );

	send_footer(req);

	/* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

/**
 * @brief deliver main html site play
 *
 * @param req
 *
 * @return error-code
 */
 esp_err_t root_html_get_handler(httpd_req_t *req )
{
	char line[128];

	send_header(req);

	httpd_resp_sendstr_chunk(req, "<iframe src=\"active_track\" height=\"50px\"></iframe>" );

	httpd_resp_sendstr_chunk(req, "<table align=\"center\"><tr>" );

	/* BACKWARD */ httpd_resp_sendstr_chunk(req, "<td width=\"35\"><a onclick=\"previousTrack()\"><img src=\"/img/previous.svg\" height=\"20\"></a></td>");

	char stylePlay[30] = "";
	char styleStop[30] = "";

	if ( audio_element_get_state(ftcSoundBar.pipeline.i2s_stream_writer) != AEL_STATE_RUNNING) {
		strcpy( styleStop, "style=\"display:none\"" );
	} else {
		strcpy( stylePlay, "style=\"display:none\"" );
	}

	httpd_resp_sendstr_chunk(req, "<td width=\"35\">");
	sprintf( line, "<div id=\"play\" %s><a onclick=\"playTrack()\"><img src=\"/img/play.svg\" height=\"20\"></img></a></div>", stylePlay );
	httpd_resp_sendstr_chunk(req, line );
	sprintf( line, "<div id=\"stop\" %s><a onclick=\"stopTrack()\"><img src=\"/img/stop.svg\" height=\"20\"></img></a></div>", styleStop );
	httpd_resp_sendstr_chunk(req, line );
	httpd_resp_sendstr_chunk(req, "</td>");

	/* FORWARD  */ httpd_resp_sendstr_chunk(req, "<td width=\"35\"><a onclick=\"nextTrack()\"><img src=\"/img/next.svg\" height=\"20\"></a></td>");

	// REPEAT
	httpd_resp_sendstr_chunk(req, "<td width=\"35\"><a onclick=\"modeRepeat()\"><svg id=\"repeat\" viewBox=\"0 0 512 512\" width=\"20\" height=\"20\" ");
	if ( ftcSoundBar.mode == MODE_REPEAT ) {
		httpd_resp_sendstr_chunk(req, "fill=\"white\"");
	} else {
		httpd_resp_sendstr_chunk(req, "fill=\"#989898\"");
	}
	httpd_resp_sendstr_chunk(req, "><path d=\"M256.455 8c66.269.119 126.437 26.233 170.859 68.685l35.715-35.715C478.149 25.851 504 36.559 504 57.941V192c0 13.255-10.745 24-24 24H345.941c-21.382 0-32.09-25.851-16.971-40.971l41.75-41.75c-30.864-28.899-70.801-44.907-113.23-45.273-92.398-.798-170.283 73.977-169.484 169.442C88.764 348.009 162.184 424 256 424c41.127 0 79.997-14.678 110.629-41.556 4.743-4.161 11.906-3.908 16.368.553l39.662 39.662c4.872 4.872 4.631 12.815-.482 17.433C378.202 479.813 319.926 504 256 504 119.034 504 8.001 392.967 8 256.002 7.999 119.193 119.646 7.755 256.455 8z\"></path></svg></a></td>");

	// SHUFFLE
	httpd_resp_sendstr_chunk(req, "<td width=\"35\"><a onclick=\"modeShuffle()\"><svg id=\"shuffle\" viewBox=\"0 0 512 512\" width=\"25\" height=\"25\"");
	if ( ftcSoundBar.mode == MODE_SHUFFLE ) {
		httpd_resp_sendstr_chunk(req, "fill=\"white\"");
	} else {
		httpd_resp_sendstr_chunk(req, "fill=\"#989898\"");
	}
	httpd_resp_sendstr_chunk(req, "><path d=\"M504.971 359.029c9.373 9.373 9.373 24.569 0 33.941l-80 79.984c-15.01 15.01-40.971 4.49-40.971-16.971V416h-58.785a12.004 12.004 0 0 1-8.773-3.812l-70.556-75.596 53.333-57.143L352 336h32v-39.981c0-21.438 25.943-31.998 40.971-16.971l80 79.981zM12 176h84l52.781 56.551 53.333-57.143-70.556-75.596A11.999 11.999 0 0 0 122.785 96H12c-6.627 0-12 5.373-12 12v56c0 6.627 5.373 12 12 12zm372 0v39.984c0 21.46 25.961 31.98 40.971 16.971l80-79.984c9.373-9.373 9.373-24.569 0-33.941l-80-79.981C409.943 24.021 384 34.582 384 56.019V96h-58.785a12.004 12.004 0 0 0-8.773 3.812L96 336H12c-6.627 0-12 5.373-12 12v56c0 6.627 5.373 12 12 12h110.785c3.326 0 6.503-1.381 8.773-3.812L352 176h32z\"></path></svg></a></td>");

	/* V-Down   */ httpd_resp_sendstr_chunk(req, "<td width=\"35\"><a onclick=\"volume(-10)\"><img src=\"/img/volumedown.svg\" height=\"20\"></img></a></td>");

	/* V-Up     */ httpd_resp_sendstr_chunk(req, "<td width=\"35\"><a onclick=\"volume(+10)\"><img src=\"/img/volumeup.svg\" height=\"20\"></img></a></td>");
	/* setup    */ httpd_resp_sendstr_chunk(req, "<td width=\"35\"><a href=\"setup\"><img src=\"/img/cog.svg\" height=\"20\"></img></a></td>" );
	httpd_resp_sendstr_chunk(req, "</tr></table>" );

	httpd_resp_sendstr_chunk(req, "<hr>" );
    httpd_resp_sendstr_chunk(req, "<h3 align=\"center\">Playlist</h3>");

    httpd_resp_sendstr_chunk(req, "<table>" );

    for (int i=0; i < ftcSoundBar.playList.getTracks(); i++) {

	  httpd_resp_sendstr_chunk(req, "<tr>" );

	  sprintf( line, "<td><a onclick=\"play(%d)\" class=\"select\">%03d %s</a></td></tr>", i+1, i+1, ftcSoundBar.playList.getTrack(i) );
	  httpd_resp_sendstr_chunk(req, line);

	  httpd_resp_sendstr_chunk(req, "</tr>" );
	}

	httpd_resp_sendstr_chunk(req, "</table>" );

	send_footer(req);

	/* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

#define TAGAPI "::API"

static esp_err_t track_get_handler(httpd_req_t *req)
{	char tag[20];

    ESP_LOGI( TAGAPI, "GET track" );

    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "tracks", ftcSoundBar.playList.getTracks() );
    cJSON_AddNumberToObject(root, "active_track", ftcSoundBar.playList.getActiveTrackNr() );

    cJSON_AddNumberToObject(root, "state", audio_element_get_state(ftcSoundBar.pipeline.i2s_stream_writer) );

    for (int i=0; i < ftcSoundBar.playList.getTracks(); i++) {
          sprintf( tag, "track#%d", i );
          cJSON_AddStringToObject(root, tag, ftcSoundBar.playList.getTrack(i) );
    }

    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);

    ESP_LOGI( TAGAPI, "GET track: %s", sys_info);

    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t volume_get_handler(httpd_req_t *req)
{
	ESP_LOGI( TAGAPI, "GET volume" );

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    int volume;
    audio_hal_get_volume(ftcSoundBar.pipeline.board_handle->audio_hal, &volume);
    cJSON_AddNumberToObject(root, "volume", volume );
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);

    ESP_LOGI( TAGAPI, "GET volume: %s", sys_info);

    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t mode_get_handler(httpd_req_t *req)
{
	httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "mode", ftcSoundBar.mode );
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);

    ESP_LOGI( TAGAPI, "GET mode: %s", sys_info);

    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static char *getBody(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((http_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return NULL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return NULL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    return buf;
}

static esp_err_t play_post_handler(httpd_req_t *req) {

	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST play: <null>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST play: %s", body);

    cJSON *root = cJSON_Parse(body);
    if ( root == NULL ) { return ESP_FAIL; }

    cJSON *JSONtrack = cJSON_GetObjectItem(root, "track");
    if ( JSONtrack != NULL ) {
    	int track = JSONtrack->valueint;
    	ftcSoundBar.playList.setActiveTrackNr(track-1);
    }

    ftcSoundBar.pipeline.play( ftcSoundBar.playList.getActiveTrack(), ftcSoundBar.playList.getActiveFiletype() );
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t config_post_handler(httpd_req_t *req) {

	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST config: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST config: %s", body);

    cJSON *root = cJSON_Parse(body);
    if ( root == NULL ) { return ESP_FAIL; }

    char *wifi_ssid = cJSON_GetObjectItem(root, "wifi_ssid")->valuestring;
    char *wifi_password = cJSON_GetObjectItem(root, "wifi_password")->valuestring;
    char *txt_ap_mode = cJSON_GetObjectItem(root, "txt_ap_mode")->valuestring;
    strcpy( (char *)ftcSoundBar.WIFI_SSID, wifi_ssid );
    strcpy( (char *)ftcSoundBar.WIFI_PASSWORD, wifi_password);
    ftcSoundBar.TXT_AP_MODE = ( strcmp( txt_ap_mode, "true" ) == 0 );
    ESP_LOGI( TAG, "TXT_AP_MODE: %s",txt_ap_mode);

    write_config_file();

    cJSON_Delete(root);

    xTaskCreate(&task_reboot, "reboot", 512, NULL, 5, NULL );

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t ota_post_handler(httpd_req_t *req) {

	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST ota: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST ota: %s", body);

    cJSON *root = cJSON_Parse(body);
    if ( root == NULL ) { return ESP_FAIL; }
    cJSON_Delete(root);

    xTaskCreate(&task_ota, "ota", 8192, NULL, 5, NULL );

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}


static esp_err_t volume_post_handler(httpd_req_t *req)
{
	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST volumne: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST volume: %s", body);

    cJSON *root = cJSON_Parse(body);
    cJSON *JSONvolume = cJSON_GetObjectItem(root, "volume");
    cJSON *JSONrelvolume = cJSON_GetObjectItem(root, "relvolume");

    if ( JSONvolume != NULL ) {
    	int vol = JSONvolume->valueint;
    	volume( vol );
    } else if ( JSONrelvolume != NULL ) {
    	int relvol = JSONrelvolume->valueint;
    	if ( relvol > 0 ) {
    		volume( INC_VOLUME );
    	} else {
    		volume( DEC_VOLUME );
    	}

    }
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t previous_post_handler(httpd_req_t *req)
{
	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST previous: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST previous: %s", body);

    ftcSoundBar.playList.prevTrack();
    ftcSoundBar.pipeline.play( ftcSoundBar.playList.getActiveTrack(), ftcSoundBar.playList.getActiveFiletype() );

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t next_post_handler(httpd_req_t *req)
{
	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST next: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST next: %s", body);

    ftcSoundBar.playList.nextTrack();
    ftcSoundBar.pipeline.play( ftcSoundBar.playList.getActiveTrack(), ftcSoundBar.playList.getActiveFiletype() );

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t stop_post_handler(httpd_req_t *req)
{
	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST stop: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST stop: %s", body);

	ftcSoundBar.pipeline.stop();

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t mode_post_handler(httpd_req_t *req)
{
	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST mode: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST mode: %s", body);

    cJSON *root = cJSON_Parse(body);
    int mode = cJSON_GetObjectItem(root, "mode")->valueint;
    ftcSoundBar.mode = mode;
	if ( audio_element_get_state(ftcSoundBar.pipeline.i2s_stream_writer) != AEL_STATE_RUNNING) {
		ftcSoundBar.pipeline.play( ftcSoundBar.playList.getActiveTrack(), ftcSoundBar.playList.getActiveFiletype() );
	}

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t pause_post_handler(httpd_req_t *req)
{
	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST pause: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST pause: %s", body);

    audio_pipeline_pause(ftcSoundBar.pipeline.pipeline);

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t resume_post_handler(httpd_req_t *req)
{
	char *body = getBody(req);
	if (body==NULL) {
		ESP_LOGI( TAGAPI, "POST resume: <NULL>");
		return ESP_FAIL;
	}

	ESP_LOGI( TAGAPI, "POST resume: %s", body);

    audio_pipeline_resume(ftcSoundBar.pipeline.pipeline);

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

#define TAGWEB "WEBSERVER"

/* Function to start the web server */
esp_err_t start_web_server( const char *base_path )
{

    http_server_context_t *http_context = (http_server_context_t*)calloc(1, sizeof(http_server_context_t));
    strlcpy(http_context->base_path, base_path, sizeof(http_context->base_path));

    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    config.stack_size = 20480;

    ESP_LOGI(TAGWEB, "Starting HTTP Server");
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAGWEB, "Failed to start web server!");
        return ESP_FAIL;
    }

    // /
    httpd_uri_t root_html = { .uri = "/", .method = HTTP_GET, .handler   = root_html_get_handler , .user_ctx = NULL };
    httpd_register_uri_handler(server, &root_html);

    // setup
    httpd_uri_t setup_html = { .uri = "/setup", .method = HTTP_GET, .handler = setup_html_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &setup_html);

    // active_track
    httpd_uri_t active_track_html = { .uri = "/active_track", .method = HTTP_GET, .handler = active_track_html_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &active_track_html);

    // styles.css
    httpd_uri_t styles_css = { .uri = "/styles.css", .method = HTTP_GET, .handler = styles_css_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &styles_css);

    // favicon
    httpd_uri_t favico = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favico_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &favico);

    // img/*
    httpd_uri_t img = { .uri = "/img/*", .method = HTTP_GET, .handler = img_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &img);

    // API
    httpd_uri_t track_get_uri = {.uri = "/api/v1/track", .method = HTTP_GET, .handler = track_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &track_get_uri);

    httpd_uri_t play_post_uri = { .uri = "/api/v1/track/play", .method = HTTP_POST, .handler = play_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &play_post_uri);

    httpd_uri_t config_post_uri = { .uri = "/api/v1/config",.method = HTTP_POST, .handler = config_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &config_post_uri);

    httpd_uri_t ota_post_uri = { .uri = "/api/v1/ota", .method = HTTP_POST, .handler = ota_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &ota_post_uri);

    httpd_uri_t volume_post_uri = { .uri = "/api/v1/volume", .method = HTTP_POST, .handler = volume_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &volume_post_uri);

    httpd_uri_t volume_get_uri = { .uri = "/api/v1/volume", .method = HTTP_GET, .handler = volume_get_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &volume_get_uri);

    httpd_uri_t previous_post_uri = { .uri = "/api/v1/track/previous", .method = HTTP_POST, .handler = previous_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &previous_post_uri);

    httpd_uri_t next_post_uri = { .uri = "/api/v1/track/next", .method = HTTP_POST, .handler = next_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &next_post_uri);

    httpd_uri_t stop_post_uri = { .uri = "/api/v1/track/stop", .method = HTTP_POST, .handler = stop_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &stop_post_uri);

    httpd_uri_t mode_post_uri = { .uri = "/api/v1/mode", .method = HTTP_POST, .handler = mode_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &mode_post_uri);

    httpd_uri_t mode_get_uri = { .uri = "/api/v1/mode", .method = HTTP_GET, .handler = mode_get_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &mode_get_uri);

    httpd_uri_t pause_post_uri = { .uri = "/api/v1/track/pause", .method = HTTP_POST, .handler = pause_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &pause_post_uri);

    httpd_uri_t resume_post_uri = { .uri = "/api/v1/track/resume", .method = HTTP_POST, .handler = resume_post_handler, .user_ctx = http_context };
    httpd_register_uri_handler(server, &resume_post_uri);

    return ESP_OK;
}

/******************************************************************
 *
 * wifi
 *
 ******************************************************************/

#define TAGWIFI "WIFI"

esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
	ESP_LOGI(TAGWIFI, "wifi_event_handler");

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAGWIFI, "SYSTEM_EVENT_STA_START");
        ESP_ERROR_CHECK( esp_wifi_connect() );
        ESP_ERROR_CHECK( tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, ftcSoundBar.HOSTNAME) );

    	// set static IP?
        if ( ftcSoundBar.TXT_AP_MODE) {
        	// DHCP off
        	ESP_ERROR_CHECK( tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));

        	// set 192.168.8.1
        	tcpip_adapter_ip_info_t myip;
        	IP4_ADDR( &myip.gw, 192,168,8,1 );
        	IP4_ADDR( &myip.ip, 192,168,8,100 );
        	IP4_ADDR( &myip.netmask, 255,255,255,0);

        	ESP_ERROR_CHECK( tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &myip));
        }

        break;
    case SYSTEM_EVENT_STA_GOT_IP:
    	ESP_LOGI(TAGWIFI, "SYSTEM_EVENT_STA_GOT_IP");
        ESP_LOGI(TAGWIFI, "Got IP: '%s'",
                ip4addr_ntoa((ip4_addr_t*) &event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAGWIFI, "SYSTEM_EVENT_STA_DISCONNECTED");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    default:
        break;
    }

    return ESP_OK;
}

/******************************************************************
 *
 * keyboard
 *
 ******************************************************************/

#define TAGKEY "KEYBOARD"

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    /* Handle touch pad events
           to start, pause, resume, finish current song and adjust volume
        */
    int player_volume;
    audio_hal_get_volume(ftcSoundBar.pipeline.board_handle->audio_hal, &player_volume);

    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        ESP_LOGI(TAGKEY, "[ * ] input key id is %d", (int)evt->data);
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_PLAY: {
                ESP_LOGI(TAGKEY, "[ * ] [Play] input key event");
                audio_element_state_t el_state = audio_element_get_state(ftcSoundBar.pipeline.i2s_stream_writer);
                switch (el_state) {
                    case AEL_STATE_INIT :
                        ESP_LOGI(TAGKEY, "[ * ] Starting audio pipeline");
                        audio_pipeline_run(ftcSoundBar.pipeline.pipeline);
                        break;
                    case AEL_STATE_RUNNING :
                        ESP_LOGI(TAGKEY, "[ * ] Pausing audio pipeline");
                        audio_pipeline_pause(ftcSoundBar.pipeline.pipeline);
                        break;
                    case AEL_STATE_PAUSED :
                        ESP_LOGI(TAGKEY, "[ * ] Resuming audio pipeline");
                        audio_pipeline_resume(ftcSoundBar.pipeline.pipeline);
                        break;
                    default :
                        ESP_LOGI(TAGKEY, "[ * ] Not supported state %d", el_state);
                }
                break; }
            case INPUT_KEY_USER_ID_SET:
                ESP_LOGI(TAGKEY, "[ * ] [Set] input key event");
                ESP_LOGI(TAGKEY, "[ * ] Stopped, advancing to the next song");
                ftcSoundBar.playList.nextTrack();
                ftcSoundBar.pipeline.play( ftcSoundBar.playList.getActiveTrack(), ftcSoundBar.playList.getActiveFiletype() );
                break;
            case INPUT_KEY_USER_ID_VOLUP:
                ESP_LOGI(TAGKEY, "[ * ] [Vol+] input key event");
                volume( INC_VOLUME );
                break;
            case INPUT_KEY_USER_ID_VOLDOWN:
                ESP_LOGI(TAGKEY, "[ * ] [Vol-] input key event");
                volume( DEC_VOLUME );
                break;
        }
    }

    return ESP_OK;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ftcSoundBar_init( &ftcSoundBar );

    ESP_LOGI(TAG, "ftMusicBox startup" );
    ESP_LOGI(TAG, "(C) 2020 Idee: Oliver Schmiel, Programmierung: Christian Bergschneider, Stefan Fuss" );

    xTaskCreate(&task_blinky, "blinky", 512, NULL, 5, &(ftcSoundBar.xBlinky) );

    nvs_flash_init();

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Initialize and start peripherals");
    audio_board_key_init(set);
    audio_board_sdcard_init(set);

    ESP_LOGI(TAG, "[1.2] Set up a sdcard playlist and scan sdcard music save to it");
    ftcSoundBar.playList.readDir( "/sdcard" );

    ESP_LOGI(TAG, "[2.0] Initialize wifi" );
    read_config_file();
    wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
    wifi_config_t sta_config = {};
    memcpy( sta_config.sta.ssid, ftcSoundBar.WIFI_SSID, 32);
    memcpy( sta_config.sta.password, ftcSoundBar.WIFI_PASSWORD, 64);
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connect to Wifi ! Start to Connect to Server....");
    if ( ftcSoundBar.xBlinky != NULL ) { vTaskDelete( ftcSoundBar.xBlinky ); }
	gpio_set_level((gpio_num_t)BLINK_GPIO, 1);

	mdns_init();
	mdns_hostname_set( ftcSoundBar.HOSTNAME );

    ESP_LOGI(TAG, "[3.0] Start codec chip");
    ftcSoundBar.pipeline.StartCodec();
    ftcSoundBar.pipeline.build( FILETYPE_MP3 );

    ESP_LOGI(TAG, "[4.0] Create and start input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = _INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void *)ftcSoundBar.pipeline.board_handle);

    ESP_LOGI(TAG, "[6.0] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[6.1] Listen for all pipeline events");
    audio_pipeline_set_listener(ftcSoundBar.pipeline.pipeline, evt);

    ESP_LOGI(TAG, "[7.0] Press the keys to control music player:");
    ESP_LOGI(TAG, "      [Play] to start, pause and resume, [Set] next song.");
    ESP_LOGI(TAG, "      [Vol-] or [Vol+] to adjust volume.");

    ESP_LOGI(TAG, "[8.0] Start Web Server");
    ESP_ERROR_CHECK( start_web_server( "localhost"  ) );

    ESP_LOGI(TAG, "[9.0] Everything started");

    volume(25);

    while (1) {
    	vTaskDelay( 10000 );
    }

    while (1) {

        /* Handle event interface messages from pipeline
           to set music info and to advance to the next song
        */
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
        	continue;
            // Set music info for a new song to be played
            if (msg.source == (void *) ftcSoundBar.pipeline.decoder
                && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
                audio_element_info_t music_info = AUDIO_ELEMENT_INFO_DEFAULT();
                audio_element_getinfo(ftcSoundBar.pipeline.decoder, &music_info);
                ESP_LOGI(TAG, "[ * ] Received music info from decoder, sample_rates=%d, bits=%d, ch=%d duration=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels, music_info.duration);
                audio_element_setinfo(ftcSoundBar.pipeline.i2s_stream_writer, &music_info);
                i2s_stream_set_clk(ftcSoundBar.pipeline.i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
                continue;
            }
            // Advance to the next song when previous finishes
            if (msg.source == (void *) ftcSoundBar.pipeline.i2s_stream_writer
                && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                audio_element_state_t el_state = audio_element_get_state(ftcSoundBar.pipeline.i2s_stream_writer);
                if (el_state == AEL_STATE_FINISHED) {
                    ESP_LOGI(TAG, "[ * ] Finished, advancing to the next song");

         		    switch (ftcSoundBar.mode)
         			 {
         		   	   case MODE_SINGLE_TRACK:
         		   		   ESP_LOGI(TAG, "SINGLE_TRACK");
         		   		   break;
         		   	   case MODE_SHUFFLE:

         		   		   ftcSoundBar.playList.setRandomTrack();
         		   		   ESP_LOGI(TAG, "SHUFFLE next track=%d", ftcSoundBar.playList.getActiveTrackNr() );
         		   		   ftcSoundBar.pipeline.stop();
         		   		   ftcSoundBar.pipeline.play( ftcSoundBar.playList.getActiveTrack(), ftcSoundBar.playList.getActiveFiletype() );
         		   		   break;
         		   	   case MODE_REPEAT:
         		   		   ESP_LOGI(TAG, "REPEAT");
         		   		   ESP_LOGI(TAG, "%d", audio_element_get_state(ftcSoundBar.pipeline.i2s_stream_writer) );
         		   		   //ftcSoundBar.pipeline.stop();
         		   		   ftcSoundBar.pipeline.play( ftcSoundBar.playList.getActiveTrack(), ftcSoundBar.playList.getActiveFiletype() );
         		   		   break;
         		 	 }
                }
                continue;
            }
        }
    }

}
