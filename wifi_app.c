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

//#undef ESP_ERROR_CHECK
//#define ESP_ERROR_CHECK(x)   do { esp_err_t _err_rc = (x); if (_err_rc != ESP_OK) { ESP_LOGE("err", "esp_err_t = %d", _err_rc); assert(0 && #x);} } while(0);

static EventGroupHandle_t wifi_app_event_group;


const int WIFI_CONNECTING_USING_SAVED_CREDS_BIT = BIT0;
const int WIFI_CONNECTING_FROM_SMARTCONFIG = BIT1;
const int WIFI_USER_CLEAR_SETTINGS = BIT2;



static const char TAG[] = "wifi_app";

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
			break;

		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");

			break;

		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
//			wifi_event_sta_disconnected_t *sta_disconnected = (wifi_event_sta_disconnected_t*)malloc(sizeof(wifi_event_sta_disconnected_t));
//			*sta_disconnected = *((wifi_event_sta_disconnected_t)event_data);
			if(global_retry < MAX_CONNECTION_RETRIES)
			{
				esp_wifi_connect();
				global_retry++;
				printf("TRYING TO CONNECT %d", global_retry);
			}
			else
			{
				ESP_LOGI(TAG, "ATTEMPTING SMARTCONFIG");
				wifi_app_send_message(WIFI_CANNOT_CONNECT);
				//start_sc();
			}
			//ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
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

	//Event loop for Wifi driver
	//ESP_ERROR_CHECK(esp_event_loop_create_default());

	//Event handler for the connection
//	esp_event_handler_t wifi_event;
//	esp_event_handler_t ip_event;

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_app_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_app_event_handler, NULL));

}

static void wifi_app_event_handler_init_saved_credentials(void){

	//Event loop for Wifi driver
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	//Event handler for the connection
//	esp_event_handler_t wifi_event;
//	esp_event_handler_t ip_event;

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_app_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_app_event_handler, NULL));

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


//	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
//
//	tcpip_adapter_ip_info_t server_info;
//	//memset(&server_info, 0, sizeof(server_info));
//	IP4_ADDR(&server_info.ip, 192,168,1,199);
//	IP4_ADDR(&server_info.netmask, 255,255,255,0);
//	IP4_ADDR(&server_info.gw, 192,168,1,1);
//	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &server_info);

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

	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);

	tcpip_adapter_ip_info_t server_info;
	//memset(&server_info, 0, sizeof(server_info));
	IP4_ADDR(&server_info.ip, 192,168,1,199);
	IP4_ADDR(&server_info.netmask, 255,255,255,0);
	IP4_ADDR(&server_info.gw, 192,168,1,1);
	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &server_info);
	//ESP_LOGI(TAG, "LINE 147 WIFI_APP.c");
	ESP_LOGI(TAG, &wifi_config->sta.ssid);
	ESP_LOGI(TAG, &wifi_config->sta.password);

	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, get_wifi_config()));
	//ESP_ERROR_CHECK(esp_wifi_connect());
	esp_err_t connect_result = esp_wifi_connect();
	if (connect_result == ESP_OK)
	{
		if (load_mqtt_topic())
		{

			mqtt_main();
		}
		else
		{
			wifi_app_send_message(WIFI_SERVER_START);
		}

//		wifi_app_send_message(WIFI_APP_STA_CONNECTED);

	}
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

				if (app_nvs_load_sta_creds())
				{

					ESP_LOGI(TAG, "Credentials Loaded");
					xEventGroupSetBits(wifi_app_event_group, WIFI_CONNECTING_USING_SAVED_CREDS_BIT);
					wifi_app_event_handler_init_saved_credentials();

					//Initialize the tcpip stack
					wifi_app_default_wifi_init();

					//Start Wifi
					ESP_ERROR_CHECK(esp_wifi_start());

					//Attempt connection
					wifi_app_connect_sta();

					//set number of retries to 0
					global_retry = 0;
					wifi_app_send_message(WIFI_APP_STA_CONNECTED);
//					wifi_app_send_message(WIFI_APP_WIFI_READY);

					if (load_mqtt_topic()){
						bool topic_found = load_mqtt_topic();
						ESP_LOGI(TAG, "MQTT TOPIC FOUND");
					}
				}

				else
				{
					ESP_LOGI(TAG, "No Credentials Found");

//					create_led_blink_tasks();
//					led_send_message(LED_BLINK_SLOW_START);
					start_sc();
				}
				//wifi_app_send_message(WIFI_SERVER_START);
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
				eventBits = xEventGroupGetBits(wifi_app_event_group);



				if (eventBits & WIFI_CONNECTING_USING_SAVED_CREDS_BIT)
				{
					xEventGroupClearBits(wifi_app_event_group, WIFI_CONNECTING_USING_SAVED_CREDS_BIT);
				}

				else
				{
					app_nvs_save_sta_creds();
				}

				if (eventBits & WIFI_CONNECTING_FROM_SMARTCONFIG)
				{
					xEventGroupClearBits(wifi_app_event_group, WIFI_CONNECTING_FROM_SMARTCONFIG);
				}
				break;

			case WIFI_APP_WIFI_READY:
				ESP_LOGI(TAG, "189 WIFI_APP_WIFI_READY");

				xEventGroupSetBits(wifi_app_event_group, WIFI_CONNECTING_FROM_SMARTCONFIG);
				//set_blink_duration(500);
				wifi_app_event_handler_init();

				//Initialize the tcpip stack
				wifi_app_default_wifi_init();

				//Start Wifi
				ESP_ERROR_CHECK(esp_wifi_start());

				//Attempt connection
				wifi_app_connect_sta();

				//set number of retries to 0
				global_retry = 0;
				wifi_app_send_message(WIFI_APP_STA_CONNECTED);
				//tell server about the connection attempt
				//http_server_send_message(HTTP_MSG_WIFI_CONNECT_INIT);
				break;

			case WIFI_APP_STA_DISCONNECTED:
				ESP_LOGI(TAG, "WIFI_APP_STA_DISCONNECTED");
				eventBits = xEventGroupGetBits(wifi_app_event_group);
				if(eventBits & WIFI_CONNECTING_USING_SAVED_CREDS_BIT)
				{
					ESP_LOGI(TAG, "DISCONNECT : Attempt using saved creds");
					xEventGroupClearBits(wifi_app_queue_handle, WIFI_CONNECTING_USING_SAVED_CREDS_BIT);
					clear_sta_creds();
				}
				else if (eventBits & WIFI_CONNECTING_FROM_SMARTCONFIG)
				{
					ESP_LOGI(TAG, "Disconnect : Attempt from Smart Config");
					xEventGroupClearBits(wifi_app_event_group, WIFI_CONNECTING_FROM_SMARTCONFIG);
					start_sc();

				}
				break;

			case WIFI_APP_USER_CLEAR_SETTINGS:
					ESP_LOGI(TAG, "USER CLEAR SETTINGS");
					global_retry = MAX_CONNECTION_RETRIES;
					ESP_ERROR_CHECK(esp_wifi_disconnect());
					clear_sta_creds();
					start_sc();
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


