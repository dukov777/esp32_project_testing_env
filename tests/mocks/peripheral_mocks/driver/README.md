# Peripheral mock: `driver`

Shadows the ESP-IDF `driver` component when this directory is copied into
a test app's `components/` (highest precedence) at CMake configure time.

## Wiring it into a test app

Mirror the `component_B` mock-injection pattern: in the test app's root
`CMakeLists.txt`, before `project()`:

```cmake
file(REMOVE_RECURSE "${CMAKE_CURRENT_LIST_DIR}/components/driver")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../mocks/peripheral_mocks/driver/"
     DESTINATION "${CMAKE_CURRENT_LIST_DIR}/components/driver/")
```

`tests/apps/*/components/` is git-ignored; the copy is regenerated each
configure. Edit the canonical source under
`mocks/peripheral_mocks/driver/`, then re-run `idf.py … reconfigure` (or
`build`).

## Adding more headers

Append to `MOCK_HEADER_FILES` in `CMakeLists.txt`:

```cmake
MOCK_HEADER_FILES
    "${original_dir}/include/driver/gpio.h"
    "${original_dir}/include/driver/i2c.h"
    "${original_dir}/include/driver/spi_master.h"
```

Headers may pull in additional component types — extend `REQUIRES` as
needed (e.g., `esp_pm` for power-management types). `hal` and `soc` are
already required because `driver/gpio.h` transitively pulls types from
`hal/gpio_types.h` and `soc/gpio_num.h`.

## Adding mocks for other peripherals

Create a sibling directory whose name matches the IDF component to shadow
(e.g., `mocks/peripheral_mocks/esp_wifi/`). Mirror this CMakeLists. **The
directory name must match the original component name exactly.**
