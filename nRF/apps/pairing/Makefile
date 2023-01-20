PROJECT_NAME = $(shell basename "$(realpath ./)")

# Configurations
NRF_IC = nrf52832
SDK_VERSION = 15
SOFTDEVICE_MODEL = s132

# Source and header files
APP_HEADER_PATHS += .
APP_HEADER_PATHS += ./include/bls12_381
APP_HEADER_PATHS += ./include/core
APP_HEADER_PATHS += ./include/core/arch/armv6_m
APP_HEADER_PATHS += ./include/lqibe
APP_HEADER_PATHS += ./include/wkdibe

APP_SOURCE_PATHS += .
APP_SOURCES = $(notdir $(wildcard ./*.c))
APP_SOURCES += $(notdir $(wildcard ./*.a))

APP_LINKER_PATHS = .
LDLIBS += -lpairing

NRF_BASE_DIR ?= ../../../

# Include board Makefile (if any)
include $(NRF_BASE_DIR)/boards/nrf52dk/Board.mk

# Include main Makefile
include $(NRF_BASE_DIR)/make/AppMakefile.mk
