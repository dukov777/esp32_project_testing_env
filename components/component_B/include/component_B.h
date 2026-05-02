#pragma once
#include "esp_err.h"

esp_err_t component_B_init(void);
esp_err_t component_B_deinit(void);
int component_B_process(int input);
