# Board-specific configurations for the nRF52 development kit (pca10040)

# Ensure that this file is only included once
ifndef BOARD_MAKEFILE
BOARD_MAKEFILE = 1

# Get directory of this makefile
BOARD_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

# Include any files in this directory in the build process
BOARD_SOURCE_PATHS = $(BOARD_DIR)/.
BOARD_HEADER_PATHS = $(BOARD_DIR)/.
BOARD_LINKER_PATHS = $(BOARD_DIR)/.
BOARD_SOURCES = $(notdir $(wildcard $(BOARD_DIR)/./*.c))
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/platform.c)
#BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/components/libraries/crypto/backend/mbedtls/*.c)
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/ecp.c)
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/ecp_curves.c)
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/ecc.c)
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/ecdh.c)
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/ctr_drbg.c)
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/bignum.c)
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/sha512.c)
BOARD_SOURCES += $(wildcard $(BOARD_DIR)/../../../ext/nrf52x-base/sdk/nrf5_sdk_15.3.0/external/mbedtls/library/aes.c)
BOARD_AS = $(notdir $(wildcard $(BOARD_DIR)/./*.s))

# Board-specific configurations
BOARD = PCA10056
USE_BLE = 1

# Additional #define's to be added to code by the compiler
BOARD_VARS = \
	BOARD_$(BOARD)\
	USE_APP_CONFIG\
	DEBUG\
	DEBUG_NRF\

# Default SDK source files to be included
BOARD_SOURCES += \
	app_error.c\
	app_error_handler_gcc.c\
	app_scheduler.c\
	app_timer.c\
	app_uart.c\
	app_util_platform.c\
	before_startup.c\
	hardfault_handler_gcc.c\
	hardfault_implementation.c\
	nrf_assert.c\
	nrf_atomic.c\
	nrf_balloc.c\
	nrf_ble_gatt.c\
	nrf_ble_qwr.c\
	nrf_drv_twi.c\
	nrf_drv_uart.c\
	nrf_fprintf.c\
	nrf_fprintf_format.c\
	nrf_log_backend_rtt.c\
	nrf_log_backend_serial.c\
	nrf_log_backend_uart.c\
	nrf_log_default_backends.c\
	nrf_log_frontend.c\
	nrf_log_str_formatter.c\
	nrf_pwr_mgmt.c\
	nrf_memobj.c\
	nrf_ringbuf.c\
	nrf_section_iter.c\
	nrf_sdh.c\
	nrf_sdh_ble.c\
	nrf_sdh_ble.c\
	nrf_serial.c\
	nrf_strerror.c\
	nrf_queue.c\
	nrf_drv_clock.c\
	nrf_nvmc.c\
	nrfx_gpiote.c\
	nrfx_prs.c\
	nrfx_saadc.c\
	nrfx_timer.c\
	nrfx_twi.c\
	nrfx_twim.c\
	nrfx_uart.c\
	nrfx_uarte.c\
	nrfx_clock.c\
	SEGGER_RTT.c\
	SEGGER_RTT_Syscalls_GCC.c\
	SEGGER_RTT_printf.c\
	ble_advertising.c\
	ble_advdata.c\
	ble_conn_params.c\
	ble_srv_common.c\
	simple_ble.c\
	nrf_crypto_init.c\
	nrf_crypto_ecdh.c\
	nrf_crypto_error.c\

endif

