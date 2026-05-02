#pragma once
#include "esp_err.h"

esp_err_t component_A_init(void);
esp_err_t component_A_deinit(void);
esp_err_t component_A_do_work(int input, int *output);
