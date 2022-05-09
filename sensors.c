#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "i2c/i2c.h"
#include "bmp280/bmp280.h"
#include "ssid_config.h"
#include "ota-tftp.h"
#include "httpd/httpd.h"

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
	} else if (quantity == BMP280_PRESSURE) {
		return pressure;
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

// check for pressed buttons
void pcf_task(void *pvParameters) {

	uint8_t pcf_byte;

	// turn off all leds
	write_byte_pcf(leds_off);

	while (1) {

		vTaskDelay(pdMS_TO_TICKS(1000));

		pcf_byte = read_byte_pcf();

		vTaskDelay(pdMS_TO_TICKS(1000));
		printf("Temperature: %.2f C\n", read_bmp280(BMP280_TEMPERATURE));
		printf("Pressure: %.2f Pa\n", read_bmp280(BMP280_PRESSURE));

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

void user_init(void) {

	uart_set_baud(0, 115200);
	i2c_init(BUS_I2C, SCL, SDA, I2C_FREQ_100K);
	// fix i2c driver to work with MPU-9250
	gpio_enable(SCL, GPIO_OUTPUT);

	// WiFi configuration
	struct sdk_station_config config = { .ssid = WIFI_SSID, .password = WIFI_PASS, };
	sdk_wifi_station_set_auto_connect(1);
	sdk_wifi_set_opmode(STATION_MODE);
	sdk_wifi_station_set_config(&config);
	sdk_wifi_station_connect();

	// OTA configuration
	ota_tftp_init_server(TFTP_PORT);

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

	/// create http server task
	xTaskCreate(&httpd_task, "HTTP Daemon", 1000, NULL, 2, NULL);

	// create pcf task
	xTaskCreate(pcf_task, "PCF task", 1000, NULL, 2, NULL);

	// create mpu task
	xTaskCreate(mpu_task, "MPU-9250 task", 1000, NULL, 2, NULL);
}

