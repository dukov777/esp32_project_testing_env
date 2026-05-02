#include "component_B.h"
#include <stdbool.h>

static bool s_initialized = false;

esp_err_t component_B_init(void) {
    if (s_initialized) return ESP_ERR_INVALID_STATE;
    s_initialized = true;
    return ESP_OK;
}
esp_err_t component_B_deinit(void) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    s_initialized = false;
    return ESP_OK;
}
int component_B_process(int input) { return input * 2; }
