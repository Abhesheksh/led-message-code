/*
 * gpio.c
 *
 *  Created on: 16-Nov-2022
 *      Author: abhes
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_system.h"

#include "gpio_main.h"
#include "mqtt.h"

static QueueHandle_t led_queue_handle;
static EventGroupHandle_t led_event_group;
static TaskHandle_t led_task_handle = NULL;
static const char *TAG = "GPIO";

static xQueueHandle gpio_evt_queue = NULL;
static bool is_task_suspended = false;


int relayState = 0;
int ledState = 0;
bool led_blink_state = true;

BaseType_t led_send_message(led_messages_e led_msgID)
{

		led_queue_message_t led_msg;
		led_msg.led_message = led_msgID;

		return xQueueSend(led_queue_handle, &led_msg, portMAX_DELAY);

}



//static void gpio_task_example(void *arg)
//{
//    uint32_t io_num;
//
//    for (;;) {
//        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
//            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
//        }
//    }
//}

void set_led_state(bool arg){
	led_blink_state = arg;
}

bool led_state(void){
	return led_blink_state;
}

void led_blink_function(uint32_t duration){
	while(led_blink_state){
		led_blink_state = led_state();
		if(!led_blink_state){
			break;
		}
		gpio_set_level(LED, 0);
		vTaskDelay(duration/portTICK_PERIOD_MS);
		gpio_set_level(LED,1);
		vTaskDelay(duration/portTICK_PERIOD_MS);

	}
}

void suspend_led_task(void){
vTaskSuspend(led_task_handle);
is_task_suspended = true;

}


void resume_led_task(void){
vTaskResume(led_task_handle);
is_task_suspended = false;
}

void led_task(void){

	led_queue_message_t msg;
	uint32_t blink_duration = 1000;

//	led_send_message(LED_BLINK_FAST_START);

	while(1){

		if (xQueueReceive(led_queue_handle, &msg, portMAX_DELAY))
			{
				switch(msg.led_message)
				{
				case LED_BLINK_SLOW_START:
//					if(!is_task_suspended){
//						resume_led_task();
//					}
					ESP_LOGI(TAG, "LED_BLINK_SLOW_START");
					led_blink_function(1000);
					xQueueReset(led_queue_handle);
					break;

				case LED_BLINK_SLOW_STOP:
					ESP_LOGI(TAG, "LED_BLINK_SLOW_STOP");
					suspend_led_task();

//					xQueueReset(led_queue_handle);
					break;

				case LED_BLINK_FAST_START:
//					if(!is_task_suspended){
//						resume_led_task();
//					}
					ESP_LOGI(TAG, "LED_BLINK_FAST_START");
					led_blink_function(1000);
//					xQueueReset(led_queue_handle);
					break;

				case LED_BLINK_FAST_STOP:
					ESP_LOGI(TAG, "LED_BLINK_FAST_STOP");
					led_blink_function(2000);
					suspend_led_task();
//					xQueueReset(led_queue_handle);
					break;

				case LED_BLINK_STOP:
					ESP_LOGI(TAG, "LED_BLINK_STOP");
					suspend_led_task();
//					xQueueReset(led_queue_handle);
					break;

				default:
					break;
				}
			}
	}
}

void gpio_main(void)
{

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_level(RELAY, 0);
    gpio_set_level(LED, 0);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
//    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    led_queue_handle = xQueueCreate(5,sizeof(led_queue_message_t));
    led_event_group = xEventGroupCreate();

    xTaskCreate(&led_task, "led_task", 4096, NULL, 5, &led_task_handle);

    //led_send_message(LED_BLINK_SLOW_START);

}

void switchRelaybyButton()
{
	switch(relayState) {

	case 1 :
		printf("BTN CURRENT RELAY LEVEL is %d", relayState);
		printf("BTN CURRENT LED LEVEL is %d", ledState);
		gpio_set_level(RELAY, 0);
		gpio_set_level(LED, 1);
		relayState = 0;
		ledState = 1;
		mqtt_publish("false");
		printf("BTN New RELAY LEVEL is %d", relayState);
		printf("BTN New LED LEVEL is %d", ledState);
		break;

	case 0 :
		printf("BTN CURRENT RELAY LEVEL is %d", relayState);
		printf("BTN CURRENT LED LEVEL is %d", ledState);
		gpio_set_level(RELAY, 1);
		gpio_set_level(LED, 0);
		relayState = 1;
		ledState = 0;
		mqtt_publish("true");
		printf("BTN New RELAY LEVEL is %d", relayState);
		printf("BTN New LED LEVEL is %d", ledState);
		break;

	default :
		break;

	}


}

void switchRelaybyMqtt(char data[])
{

	printf("MQTT SWITCH RELAY : DATA is %s", data);
	if(strcmp(data, "true") == 0)
	{
		printf("CURRENT RELAY LEVEL is %d", gpio_get_level(RELAY));
		printf("CURRENT LED LEVEL is %d", gpio_get_level(LED));
		ESP_LOGI(TAG, "Switching on Relay & LED");
		gpio_set_level(RELAY, 1);
		gpio_set_level(LED, 0);
		printf("NEW RELAY LEVEL is %d", gpio_get_level(RELAY));
		printf("NEW LED LEVEL is %d", gpio_get_level(LED));
	}

	else if(strcmp(data, "false") == 0)

	{
		printf("CURRENT RELAY LEVEL is %d", gpio_get_level(RELAY));
		printf("CURRENT LED LEVEL is %d", gpio_get_level(LED));
		ESP_LOGI(TAG, "Switching OFF Relay & LED");
		gpio_set_level(RELAY, 0);
		gpio_set_level(LED, 1);
		printf("NEW RELAY LEVEL is %d", gpio_get_level(RELAY));
		printf("NEW LED LEVEL is %d", gpio_get_level(LED));

	}

}






