PROGRAM=sensors

PROGRAM_SRC_FILES=./sensors.c

EXTRA_COMPONENTS=extras/rboot-ota extras/i2c extras/bmp280 extras/mbedtls extras/httpd

EXTRA_CFLAGS=-DLWIP_HTTPD_CGI=1 -DLWIP_HTTPD_SSI=1 -I./fsdata

include ~/esp-open-rtos/common.mk

html:
	@echo "Generating fsdata.."
	cd fsdata && ./makefsdata

