/*
 * ota.cpp
 *
 *  Created on: 13.02.2021
 *      Author: Stefan Fuss
 */

#include <esp_flash_partitions.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <string.h>
#include <esp_peripherals.h>

#include "ota.h"
#include "blink.h"

#define SOS_FILENOTFOUND 1
#define SOS_READERROR 2
#define SOS_VERSION 3
#define SOS_BEGIN 4
#define SOS_FILESIZE 5
#define SOS_WRITE 6
#define SOS_CORRUPT 7
#define SOS_END 8
#define SOS_SETBOOTPARTITION 9

#define BUFFSIZE 1024
static char ota_write_data[BUFFSIZE + 1] = { 0 };

void ota( const char *tag, const char *firmware )
{
    esp_err_t err;

    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(tag, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(tag, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(tag, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    FILE *f;
    f = fopen(firmware, "rb");
    if ( f == NULL ) {
    	ESP_LOGE( tag, "%s not found", firmware);
    	SOS( SOS_FILENOTFOUND );
    }

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(tag, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    int binary_file_length = 0;
    bool image_header_was_checked = false;

    while (1) {

    	blink(10,5);

    	int data_read = fread( ota_write_data, 1, BUFFSIZE, f );
        ESP_LOGI( tag, "data_read=%d", data_read);

        if (data_read < 0) {
        	ESP_LOGE(tag, "Error: file data read error");
            SOS( SOS_READERROR );

        } else if (data_read > 0) {

        	if (image_header_was_checked == false) {

        		esp_app_desc_t new_app_info;
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {

                	// check current version with downloading
                    memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    ESP_LOGI(tag, "New firmware version: %s", new_app_info.version);

                    esp_app_desc_t running_app_info;
                    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                        ESP_LOGI(tag, "Running firmware version: %s", running_app_info.version);
                    }

                    const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
                    esp_app_desc_t invalid_app_info;
                    if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
                        ESP_LOGI(tag, "Last invalid firmware version: %s", invalid_app_info.version);
                    }

                    // check current version with last invalid partition
                    if (last_invalid_app != NULL) {
                        if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
                            ESP_LOGW(tag, "New version is the same as invalid version.");
                            ESP_LOGW(tag, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                            ESP_LOGW(tag, "The firmware has been rolled back to the previous version.");
                            SOS( SOS_VERSION );
                        }
                    }

                    image_header_was_checked = true;

                    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                    if (err != ESP_OK) {
                        ESP_LOGE(tag, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                        SOS( SOS_BEGIN );
                    }
                    ESP_LOGI(tag, "esp_ota_begin succeeded");

                 } else {
                     ESP_LOGE(tag, "received package is not fit len");
                     SOS( SOS_FILESIZE );
                 }

        	}

            err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK) {
            	SOS( SOS_WRITE );
            }

            binary_file_length += data_read;
            ESP_LOGD(tag, "Written image length %d", binary_file_length);

        } else if (data_read == 0) {

        	ESP_LOGD(tag, "EOF");
            break;

        }

    }

    ESP_LOGI(tag, "Total Write binary data length: %d", binary_file_length);

    fclose(f);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
    	if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
    		ESP_LOGE(tag, "Image validation failed, image is corrupted");
    		SOS( SOS_CORRUPT );
         }
         ESP_LOGE(tag, "esp_ota_end failed (%s)!", esp_err_to_name(err));
         SOS( SOS_END );
     }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        SOS( SOS_SETBOOTPARTITION );
    }

    ESP_LOGI(tag, "Flashing firmware succeded.");

    ESP_LOGI(tag, "Rebooting system in 5s");

    for (int i=0; i<5; i++) { blink(500,500); }

    esp_restart();

}

