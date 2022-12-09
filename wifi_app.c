/*
 * wifi_app.c
 *
 *  Created on: 04-Nov-2022
 *      Author: abhes
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/netdb.h"
#include "wifi_app.h"
#include "tcpip_adapter.h"
#include "nvs_flash.h"
#include "http_server.h"
#include "smartconfig.h"
#include "app_nvs.h"
#include "esp_wifi_types.h"
#include "wifi_app.h"
#include <stdio.h>
#include "nvs_flash.h"
#include "mqtt.h"
#include "gpio_main.h"


wifi_config_t *wifi_config = NULL;
mqtt_topic_t *mqtt = NULL;


static int global_retry;

static xQueueHandle wifi_app_queue_handle;

//Wifi application event handler
static void wifi_app_event_handler(void* event_handler_arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	if(event_base == WIFI_EVENT)
	{
		switch(event_id)
		{
		case WIFI_EVENT_STA_START:
			ESP_LOGI(TAG, "59 WIFI_EVENT_STA_START");
			BaseType_t message_sent = led_send_message(LED_BLINK_FAST_START);
			if (message_sent == pdTRUE){
				printf("MESSAGE SENT SUCCESSFULLY 62");
			}
				//MESSAGE IS RECEIVED BY xQUEUERECEIVE ON GPIO.C
			break;

		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");

			break;

		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
//			//Code Snipped for better readability
			break;

		case WIFI_EVENT_STA_STOP:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
			break;

		default:
			break;
		}
	}
	else if(event_base == IP_EVENT)
	{
		switch(event_id)
		{

		case IP_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
			wifi_app_send_message(IP_APP_STA_GOT_IP);
			break;
		}
	}
}


//Initializes the wifi application event handler for wifi and ip events
static void wifi_app_event_handler_init(void){

//Code Snipped for better readability

}

static void wifi_app_event_handler_init_saved_credentials(void){

//Code Snipped for better readability

}


//Initialize the TCPIP Stack

static void wifi_app_default_wifi_init(void)
{
	//Initialize NVS Storage
	ESP_ERROR_CHECK(nvs_flash_init());

	//Initialize TCPIP Stack

	tcpip_adapter_init();


	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	esp_wifi_set_mode(WIFI_MODE_STA);

}


BaseType_t wifi_app_send_message(wifi_messages_e msgID)
{
	wifi_app_queue_message_t msg;
	msg.wifi_message = msgID;
	return xQueueSend(wifi_app_queue_handle, &msg, portMAX_DELAY);

}

mqtt_topic_t* get_mqtt_topic(void)
{
	return mqtt;
}

wifi_config_t*  get_wifi_config(void)
{
	return wifi_config;
}

//Connects ESP to WIFI AP

static void wifi_app_connect_sta(void)
{

//Code Snipped for better readability
}



//main task for the wifi application
static void wifi_app_task(void *pvParameters)
{
	wifi_app_queue_message_t msg;

	EventBits_t eventBits;


	wifi_app_send_message(WIFI_LOAD_SAVED_CREDENTIALS);

	while(1){

		if (xQueueReceive(wifi_app_queue_handle, &msg, portMAX_DELAY))
		{
			switch(msg.wifi_message)
			{
			case WIFI_APP_STA_START:
				ESP_LOGI(TAG, "175 WIFI_APP_STA_START");
				break;

			case WIFI_CANNOT_CONNECT:
				ESP_LOGI(TAG, "CANNOT CONNECT TO WIFI");
				clear_sta_creds();
				wifi_app_send_message(WIFI_LOAD_SAVED_CREDENTIALS);
				break;

			case WIFI_LOAD_SAVED_CREDENTIALS:
				ESP_LOGI(TAG, "LOAD SAVED CREDENTIALS");

				//Code Snipped for better readability
				break;

			case WIFI_CONNECTED_VIA_SMARTCONFIG:

				ESP_LOGI(TAG, "Connected with Smartconfig");
				//xEventGroupSetBits(wifi_app_event_group, WIFI_CONNECTING_FROM_SMARTCONFIG);
				break;

			case WIFI_SERVER_START:
				http_server_start();
				ESP_LOGI(TAG, "WIFI_SERVER_START");
				break;

			case WIFI_APP_STA_CONNECTED:
				ESP_LOGI(TAG, "WIFI_APP_STA_CONNECTED");
//				led_send_message(LED_BLINK_FAST_STOP);
				BaseType_t message_sent2 = led_send_message(LED_BLINK_FAST_STOP);
				if (message_sent2 == pdTRUE){
					printf("MESSAGE SENT SUCCESSFULLY 311");
				}
					//THIS MESSAGE IS NOT RECEIVED BY xQUEUERECEIVE ON GPIO.C EVEN THOUGH PRINT STATEMENT IS PRINTED ON CONSOLE
				break;

		
			case IP_APP_STA_GOT_IP:
				ESP_LOGI(TAG, "IP_APP_STA_GOT_IP");
//				http_server_send_message(HTTP_MSG_WIFI_CONNECT_SUCCESS);
				break;

			default:
				break;
			}
		}
	}
}



//START WIFI RTOS Task
void wifi_app_start(void)
{
	ESP_LOGI(TAG,"STARTING WIFI APP");

	esp_log_level_set("wifi", ESP_LOG_NONE);

	nvs_flash_init();

	//Initialize GPIO
	gpio_main();

	//CREATE MESSAGE QUEUE
	wifi_app_queue_handle = xQueueCreate(5,sizeof(wifi_app_queue_message_t));

	//Allocate Memory
	wifi_config = (wifi_config_t*)malloc(sizeof(wifi_config_t));
	memset(wifi_config, 0x00, sizeof(wifi_config_t));

	mqtt = (mqtt_topic_t*)malloc(sizeof(mqtt_topic_t));
	memset(mqtt, 0x00, sizeof(mqtt_topic_t));

	//Create wifi application event group
	wifi_app_event_group = xEventGroupCreate();

	//START WIFI APPLICATION
	xTaskCreate(&wifi_app_task, "wifi_app_task", 4096, NULL, 5, NULL);



}


