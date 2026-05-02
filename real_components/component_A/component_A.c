#include "component_A.h"
#include "component_B.h"
#include <stddef.h>
#include <stdbool.h>

static bool s_initialized = false;

esp_err_t component_A_init(void) {
    if (s_initialized) return ESP_ERR_INVALID_STATE;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t component_A_deinit(void) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    s_initialized = false;
    return ESP_OK;
}

esp_err_t component_A_do_work(int input, int *output) {
    if (output == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    *output = component_B_process(input);
    return ESP_OK;
}
