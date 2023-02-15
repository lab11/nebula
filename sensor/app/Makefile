PROJECT_NAME = $(shell basename "$(realpath ./)")

# Configurations
NRF_IC = nrf52840
SDK_VERSION = 15
SOFTDEVICE_MODEL = s140

# Source and header files
APP_HEADER_PATHS += .
APP_SOURCE_PATHS += .
APP_SOURCES = $(notdir $(wildcard ./*.c))

NRF_BASE_DIR ?= ../../ext/nrf52x-base/

# Include board Makefile (if any)
include ../boards/nrf52840dk/Board.mk

# Include main Makefile
include $(NRF_BASE_DIR)/make/AppMakefile.mk

#Include includes here as well to capture app specific changes
include ../boards/nrf52840dk/Includes.mk
