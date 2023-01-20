PROJECT_NAME = $(shell basename "$(realpath ./)")

# Configurations
NRF_IC = nrf52832
SDK_VERSION = 15
SOFTDEVICE_MODEL = s132

# Source and header files
APP_HEADER_PATHS += . ./include
#APP_SOURCE_PATHS += . ./crypto ./groupsig ./logger ./math ./misc ./msg ./shim ./sys ./test ./tools ./wrappers
APP_SOURCE_PATHS += . ./crypto ./groupsig ./logger ./math ./misc ./msg ./shim ./sys
APP_SOURCES = $(notdir $(wildcard ./*.c))
APP_SOURCES += $(notdir $(wildcard ./crypto/*.c))
APP_SOURCES += $(notdir $(wildcard ./groupsig/*.c))
APP_SOURCES += $(notdir $(wildcard ./logger/*.c))
APP_SOURCES += $(notdir $(wildcard ./math/*.c))
APP_SOURCES += $(notdir $(wildcard ./misc/*.c))
APP_SOURCES += $(notdir $(wildcard ./msg/*.c))
APP_SOURCES += $(notdir $(wildcard ./shim/*.c))
APP_SOURCES += $(notdir $(wildcard ./sys/*.c))


# Include board Makefile (if any)
#include $(NRF_BASE_DIR)/boards/<BOARD_NAME_HERE>

# Include main Makefile
NRF_BASE_DIR ?= ../../nrf52x-base
include $(NRF_BASE_DIR)/make/AppMakefile.mk
