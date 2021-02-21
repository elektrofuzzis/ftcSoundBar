#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include <esp_peripherals.h>
#include <board.h>
#include "blink.h"
#include "ota.h"


extern "C" {
    void app_main(void);
}

static const char *TAG = "ftcSoundBar flash loader";
#define FIRMWARE_VERSION "v1.21"
#define FIRMWAREUPDATE "/sdcard/ftcSoundBar.bin"



void app_main(void)
{
	esp_log_level_set( "*", ESP_LOG_INFO);

	ESP_LOGI(TAG, "**********************************************************************************************");
	ESP_LOGI(TAG, "*                                                                                            *");
	ESP_LOGI(TAG, "*                    Asymetric bootloader for flashing ftcSoundBar.bin.                      *");
	ESP_LOGI(TAG, "*             (C) 2021 Oliver Schmiel, Christian Bergschneider & Stefan Fuss                 *");
	ESP_LOGI(TAG, "*                                        Version %s                                       *", FIRMWARE_VERSION);
	ESP_LOGI(TAG, "*                                                                                            *");
	ESP_LOGI(TAG, "* -> PLEASE BE PATIENT DRUING FLASH PROCESS. DON'T UNPLUG YOUR FTCSOUNDBAR UNTIL NOTICED! <- *");
	ESP_LOGI(TAG, "*                                                                                            *");
	ESP_LOGI(TAG, "**********************************************************************************************");
	ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Initialize and start peripherals");
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction((gpio_num_t)BLINK_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "[2.0] Start flashing firmware");

    ota( TAG, FIRMWAREUPDATE );

}
