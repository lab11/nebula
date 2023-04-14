
#ifndef __UTIL_H__
#define __UTIL_H__

#include "esp_err.h"

void print_chip_info();
void init_nvs();
esp_err_t init_wifi();

#endif // __UTIL_H__