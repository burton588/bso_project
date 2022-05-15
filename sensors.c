#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "i2c/i2c.h"
#include "bmp280/bmp280.h"
#include "ssid_config.h"
#include "ota-tftp.h"
#include "httpd/httpd.h"

#include <string.h>
#include <ssid_config.h>

#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>

#include <paho_mqtt_c/MQTTESP8266.h>
#include <paho_mqtt_c/MQTTClient.h>

#include <semphr.h>


/* You can use http://test.mosquitto.org/ to test mqtt_client instead
 * of setting up your own MQTT server */
#define MQTT_HOST ("test.mosquitto.org")
#define MQTT_PORT 1883

#define MQTT_USER NULL
#define MQTT_PASS NULL

SemaphoreHandle_t wifi_alive;
QueueHandle_t publish_queue;
#define PUB_MSG_LEN 16


#define PCF_ADDRESS	0x38
#define MPU_ADDRESS	0x68
#define BUS_I2C		0
#define SCL 14
#define SDA 12

//					mask	returned value
#define button1		0x20	// 0b ??0? ????
#define button2		0x10	// 0b ???0 ????
#define button3		0x80	// 0b 0??? ????
#define button4		0x40	// 0b ?0?? ????
#define clr_btn		0xf0

#define led1 		0xfe	// 0b ???? ???0
#define led2 		0xfd	// 0b ???? ??0?
#define led3 		0xfb	// 0b ???? ?0??
#define led4 		0xf7	// 0b ???? 0???
#define leds_off	0xff

#define gpio_wemos_led	2

typedef enum {
	BMP280_TEMPERATURE, BMP280_PRESSURE
} bmp280_quantity;

typedef enum {
	MPU9250_ACCEL_X = 0x3b,
	MPU9250_ACCEL_Y = 0x3d,
	MPU9250_ACCEL_Z = 0x3f,
	MPU9250_TEMP = 0x41,
	MPU9250_GYRO_X = 0x43,
	MPU9250_GYRO_Y = 0x45,
	MPU9250_GYRO_Z = 0x47
} mpu9250_quantity;

enum {
	SSI_UPTIME, SSI_FREE_HEAP, SSI_LED_STATE
};

bmp280_t bmp280_dev;

// read BMP280 sensor values
float read_bmp280(bmp280_quantity quantity) {

	float temperature, pressure;

	bmp280_force_measurement(&bmp280_dev);
	// wait for measurement to complete
	while (bmp280_is_measuring(&bmp280_dev)) {
	};
	bmp280_read_float(&bmp280_dev, &temperature, &pressure, NULL);

	if (quantity == BMP280_TEMPERATURE) {
		return temperature;
	}
	return 0;
}

// write byte to PCF on I2C bus
void write_byte_pcf(uint8_t data) {
	i2c_slave_write(BUS_I2C, PCF_ADDRESS, NULL, &data, 1);
}

// read byte from PCF on I2C bus
uint8_t read_byte_pcf() {
	uint8_t data;

	i2c_slave_read(BUS_I2C, PCF_ADDRESS, NULL, &data, 1);
	return data;
}

// print the temp consistently
void pcf_task(void *pvParameters) {

	uint8_t pcf_byte;

	// turn off all leds
	write_byte_pcf(leds_off);

	while (1) {

		vTaskDelay(pdMS_TO_TICKS(1000));

		pcf_byte = read_byte_pcf();

		vTaskDelay(pdMS_TO_TICKS(1000));
		printf("Temperature: %.2f C\n", read_bmp280(BMP280_TEMPERATURE));

		// check again after 1000 ms
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

// read 2 bytes from MPU-9250 on I2C bus
uint16_t read_bytes_mpu(mpu9250_quantity quantity) {

	// high and low byte of quantity
	uint8_t data_high, data_low;
	uint8_t register_address = (uint8_t) quantity;

	i2c_slave_read(BUS_I2C, MPU_ADDRESS, &register_address, &data_high, 1);
	register_address++;
	i2c_slave_read(BUS_I2C, MPU_ADDRESS, &register_address, &data_low, 1);

	return (data_high << 8) + data_low;
}

// check MPU-9250 sensor values
void mpu_task(void *pvParameters) {

	uint16_t threshold = 10000;

	while (1) {

		// turn off Wemos led
		gpio_write(gpio_wemos_led, 1);

		if (read_bytes_mpu(MPU9250_ACCEL_Z) < threshold)
			// turn on Wemos led
			gpio_write(gpio_wemos_led, 0);

		// check again after 100 ms
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

int32_t ssi_handler(int32_t iIndex, char *pcInsert, int32_t iInsertLen) {
	switch (iIndex) {
	case SSI_UPTIME:
		snprintf(pcInsert, iInsertLen, "%d", xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
		break;
	case SSI_FREE_HEAP:
		snprintf(pcInsert, iInsertLen, "%d", (int) xPortGetFreeHeapSize());
		break;
	case SSI_LED_STATE:
		snprintf(pcInsert, iInsertLen, (GPIO.OUT & BIT(gpio_wemos_led)) ? "Off" : "On");
		break;
	default:
		snprintf(pcInsert, iInsertLen, "N/A");
		break;
	}

	/* Tell the server how many characters to insert */
	return (strlen(pcInsert));
}

char *gpio_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
	for (int i = 0; i < iNumParams; i++) {
		if (strcmp(pcParam[i], "on") == 0) {
			uint8_t gpio_num = atoi(pcValue[i]);
			gpio_enable(gpio_num, GPIO_OUTPUT);
			gpio_write(gpio_num, true);
		} else if (strcmp(pcParam[i], "off") == 0) {
			uint8_t gpio_num = atoi(pcValue[i]);
			gpio_enable(gpio_num, GPIO_OUTPUT);
			gpio_write(gpio_num, false);
		} else if (strcmp(pcParam[i], "toggle") == 0) {
			uint8_t gpio_num = atoi(pcValue[i]);
			gpio_enable(gpio_num, GPIO_OUTPUT);
			gpio_toggle(gpio_num);
		}
	}
	return "/index.ssi";
}

char *about_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
	return "/about.html";
}

char *websocket_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
	return "/websockets.html";
}

void websocket_task(void *pvParameter) {
	struct tcp_pcb *pcb = (struct tcp_pcb *) pvParameter;

	for (;;) {
		if (pcb == NULL || pcb->state != ESTABLISHED) {
			printf("Connection closed, deleting task\n");
			break;
		}

		int uptime = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
		int heap = (int) xPortGetFreeHeapSize();
		int led = !gpio_read(gpio_wemos_led);

		/* Generate response in JSON format */
		char response[64];
		int len = snprintf(response, sizeof(response), "{\"uptime\" : \"%d\","
				" \"heap\" : \"%d\","
				" \"led\" : \"%d\"}", uptime, heap, led);
		if (len < sizeof(response))
			websocket_write(pcb, (unsigned char *) response, len, WS_TEXT_MODE);

		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

	vTaskDelete(NULL);
}

/**
 * This function is called when websocket frame is received.
 *
 * Note: this function is executed on TCP thread and should return as soon
 * as possible.
 */
void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode) {
	printf("[websocket_callback]:\n%.*s\n", (int) data_len, (char*) data);

	uint8_t response[2];
	uint16_t val;

	switch (data[0]) {
	case 'A': // ADC
		/* This should be done on a separate thread in 'real' applications */
		val = sdk_system_adc_read();
		break;
	case 'D': // Disable LED
		gpio_write(gpio_wemos_led, true);
		val = 0xDEAD;
		break;
	case 'E': // Enable LED
		gpio_write(gpio_wemos_led, false);
		val = 0xBEEF;
		break;
	default:
		printf("Unknown command\n");
		val = 0;
		break;
	}

	response[1] = (uint8_t) val;
	response[0] = val >> 8;

	websocket_write(pcb, response, 2, WS_BIN_MODE);
}

/**
 * This function is called when new websocket is open and
 * creates a new websocket_task if requested URI equals '/stream'.
 */
void websocket_open_cb(struct tcp_pcb *pcb, const char *uri) {
	printf("WS URI: %s\n", uri);
	if (!strcmp(uri, "/stream")) {
		printf("request for streaming\n");
		xTaskCreate(&websocket_task, "websocket_task", 256, (void *) pcb, 2,
		NULL);
	}
}

void httpd_task(void *pvParameters) {
	tCGI pCGIs[] = { { "/gpio", (tCGIHandler) gpio_cgi_handler }, { "/about", (tCGIHandler) about_cgi_handler }, { "/websockets",
			(tCGIHandler) websocket_cgi_handler }, };

	const char *pcConfigSSITags[] = { "uptime", // SSI_UPTIME
			"heap",   // SSI_FREE_HEAP
			"led"     // SSI_LED_STATE
			};

	/* register handlers and start the server */
	http_set_cgi_handlers(pCGIs, sizeof(pCGIs) / sizeof(pCGIs[0]));
	http_set_ssi_handler((tSSIHandler) ssi_handler, pcConfigSSITags, sizeof(pcConfigSSITags) / sizeof(pcConfigSSITags[0]));
	websocket_register_callbacks((tWsOpenHandler) websocket_open_cb, (tWsHandler) websocket_cb);
	httpd_init();

	for (;;)
		;
}

static void  topic_received(mqtt_message_data_t *md)
{
    int i;
    mqtt_message_t *message = md->message;
    printf("Received: ");
    for( i = 0; i < md->topic->lenstring.len; ++i)
        printf("%c", md->topic->lenstring.data[ i ]);

    printf(" = ");
    for( i = 0; i < (int)message->payloadlen; ++i)
        printf("%c", ((char *)(message->payload))[i]);

    printf("\r\n");
}

static void beat_task(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    char msg[PUB_MSG_LEN];
    int count = 0;

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, 10000 / portTICK_PERIOD_MS);
	float temperature = read_bmp280(BMP280_TEMPERATURE);
	printf("Temperature: %.2f C\n", temperature);
        snprintf(msg, PUB_MSG_LEN, "Temperature: %.2f C\n", temperature);
        if (xQueueSend(publish_queue, (void *)msg, 0) == pdFALSE) {
            printf("Publish queue overflow.\r\n");
        }
    }
}


static const char *  get_my_id(void)
{
    // Use MAC address for Station as unique ID
    static char my_id[13];
    static bool my_id_done = false;
    int8_t i;
    uint8_t x;
    if (my_id_done)
        return my_id;
    if (!sdk_wifi_get_macaddr(STATION_IF, (uint8_t *)my_id))
        return NULL;
    for (i = 5; i >= 0; --i)
    {
        x = my_id[i] & 0x0F;
        if (x > 9) x += 7;
        my_id[i * 2 + 1] = x + '0';
        x = my_id[i] >> 4;
        if (x > 9) x += 7;
        my_id[i * 2] = x + '0';
    }
    my_id[12] = '\0';
    my_id_done = true;
    return my_id;
}


static void mqtt_task(void *pvParameters)
{
    int ret         = 0;
    struct mqtt_network network;
    mqtt_client_t client   = mqtt_client_default;
    char mqtt_client_id[20];
    uint8_t mqtt_buf[100];
    uint8_t mqtt_readbuf[100];
    mqtt_packet_connect_data_t data = mqtt_packet_connect_data_initializer;

    mqtt_network_new( &network );
    memset(mqtt_client_id, 0, sizeof(mqtt_client_id));
    strcpy(mqtt_client_id, "ESP-");
    strcat(mqtt_client_id, get_my_id());

    while(1) {
        xSemaphoreTake(wifi_alive, portMAX_DELAY);
        printf("%s: started\n\r", __func__);
        printf("%s: (Re)connecting to MQTT server %s ... ",__func__,
               MQTT_HOST);
        ret = mqtt_network_connect(&network, MQTT_HOST, MQTT_PORT);
        if( ret ){
            printf("error: %d\n\r", ret);
            taskYIELD();
            continue;
        }
        printf("done\n\r");
        mqtt_client_new(&client, &network, 5000, mqtt_buf, 100,
                      mqtt_readbuf, 100);

        data.willFlag       = 0;
        data.MQTTVersion    = 3;
        data.clientID.cstring   = mqtt_client_id;
        data.username.cstring   = MQTT_USER;
        data.password.cstring   = MQTT_PASS;
        data.keepAliveInterval  = 10;
        data.cleansession   = 0;
        printf("Send MQTT connect ... ");
        ret = mqtt_connect(&client, &data);
        if(ret){
            printf("error: %d\n\r", ret);
            mqtt_network_disconnect(&network);
            taskYIELD();
            continue;
        }
        printf("done\r\n");
        mqtt_subscribe(&client, "/esptopic", MQTT_QOS1, topic_received);
        xQueueReset(publish_queue);

        while(1){

            char msg[PUB_MSG_LEN - 1] = "\0";
            while(xQueueReceive(publish_queue, (void *)msg, 0) ==
                  pdTRUE){
                printf("got message to publish\r\n");
                mqtt_message_t message;
                message.payload = msg;
                message.payloadlen = PUB_MSG_LEN;
                message.dup = 0;
                message.qos = MQTT_QOS1;
                message.retained = 0;
                ret = mqtt_publish(&client, "bso_project/temp", &message);
                if (ret != MQTT_SUCCESS ){
                    printf("error while publishing message: %d\n", ret );
                    break;
                }
            }

            ret = mqtt_yield(&client, 1000);
            if (ret == MQTT_DISCONNECTED)
                break;
        }
        printf("Connection dropped, request restart\n\r");
        mqtt_network_disconnect(&network);
        taskYIELD();
    }
}


static void  wifi_task(void *pvParameters)
{
    uint8_t status  = 0;
    uint8_t retries = 30;
    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    printf("WiFi: connecting to WiFi\n\r");
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    while(1)
    {
        while ((status != STATION_GOT_IP) && (retries)){
            status = sdk_wifi_station_get_connect_status();
            printf("%s: status = %d\n\r", __func__, status );
            if( status == STATION_WRONG_PASSWORD ){
                printf("WiFi: wrong password\n\r");
                break;
            } else if( status == STATION_NO_AP_FOUND ) {
                printf("WiFi: AP not found\n\r");
                break;
            } else if( status == STATION_CONNECT_FAIL ) {
                printf("WiFi: connection failed\r\n");
                break;
            }
            vTaskDelay( 1000 / portTICK_PERIOD_MS );
            --retries;
        }
        if (status == STATION_GOT_IP) {
            printf("WiFi: Connected\n\r");
            xSemaphoreGive( wifi_alive );
            taskYIELD();
        }

        while ((status = sdk_wifi_station_get_connect_status()) == STATION_GOT_IP) {
            xSemaphoreGive( wifi_alive );
            taskYIELD();
        }
        printf("WiFi: disconnected\n\r");
        sdk_wifi_station_disconnect();
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }
}

void user_init(void) {

	uart_set_baud(0, 115200);
	i2c_init(BUS_I2C, SCL, SDA, I2C_FREQ_100K);
	// fix i2c driver to work with MPU-9250
	gpio_enable(SCL, GPIO_OUTPUT);

	// WiFi configuration
	/*	
	struct sdk_station_config config = { .ssid = WIFI_SSID, .password = WIFI_PASS, };
	sdk_wifi_station_set_auto_connect(1);
	sdk_wifi_set_opmode(STATION_MODE);
	sdk_wifi_station_set_config(&config);
	sdk_wifi_station_connect();*/


	// BMP280 configuration
	bmp280_params_t params;
	bmp280_init_default_params(&params);
	params.mode = BMP280_MODE_FORCED;
	bmp280_dev.i2c_dev.bus = BUS_I2C;
	bmp280_dev.i2c_dev.addr = BMP280_I2C_ADDRESS_0;
	bmp280_init(&bmp280_dev, &params);

	// turn off Wemos led
	gpio_enable(gpio_wemos_led, GPIO_OUTPUT);
	gpio_write(gpio_wemos_led, 1);

	vSemaphoreCreateBinary(wifi_alive);
    	publish_queue = xQueueCreate(5, PUB_MSG_LEN);
	xTaskCreate(&wifi_task, "wifi_task",  256, NULL, 2, NULL);
	xTaskCreate(&beat_task, "beat_task", 256, NULL, 3, NULL);
    	xTaskCreate(&mqtt_task, "mqtt_task", 1024, NULL, 4, NULL);

}

