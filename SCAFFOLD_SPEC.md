# Claude Code Task: Scaffold ESP-IDF Test Project (v3 — corrected)

> **v3.1 corrections (2026-05-02) — these supersede the v3 text below where they conflict.**
>
> **Note (2026-05-02):** the directory `real_components/` referenced throughout this spec was renamed to `components/` post-scaffold. The body of the spec still says `real_components/`; the live tree uses `components/`. Mentally substitute. (Project root `components/` is not the same as IDF's "project components" precedence concept — projects live under `test_apps/`, and each one has its own `<test_app>/components/` for mock injection. The repo-root `components/` is just an `EXTRA_COMPONENT_DIRS` entry.)
>
> **C1 — Test components moved out of component dirs.** IDF's component scanner treats an `EXTRA_COMPONENT_DIRS` entry that itself has a `CMakeLists.txt` as a single component; subdirectories under it are NOT scanned. So `real_components/component_A/test_a/` would never register, and `set(COMPONENTS … test_a)` plus `REQUIRES … test_a` would both fail at configure.
>
> Fix: introduce a sibling `test_components/` tree.
> ```
> test_components/
>   ├── test_a/    (was real_components/component_A/test_a/)
>   ├── test_b/    (was real_components/component_B/test_b/)
>   └── test_int/  (was real_components/integration_tests/test_int/)
> ```
> Each test app's `EXTRA_COMPONENT_DIRS` adds `…/test_components` alongside `…/real_components`. Test CMakeLists are unchanged (still `REQUIRES unity component_X` and `WHOLE_ARCHIVE`). Test source files are unchanged.
>
> **C2 — Drop the `integration_tests` config-only parent.** It registered with `INCLUDE_DIRS ""` (empty string is invalid) and provided no value. With C1, `test_int` lives directly under `test_components/`, no parent component needed. Remove `integration_tests` from any `set(COMPONENTS …)` list (only `test_int` remains).
>
> **C3 — `TEST_COMPONENTS` must be passed via environment, not CMake variable.** IDF's component requirements pre-scan (`__component_get_requirements`) runs as a separate `cmake -P` subprocess that does not inherit parent CMakeLists variable scope. A plain `set(TEST_COMPONENTS …)` in the test app's root CMakeLists is invisible to `test_common/test_main/CMakeLists.txt` when the pre-scan processes it, so the FATAL_ERROR fires at configure ("TEST_COMPONENTS not set").
>
> Fix: use `set(ENV{TEST_COMPONENTS} "test_a")` in the test app's root CMakeLists (env vars survive `cmake -P` subprocess fork), and have `test_main/CMakeLists.txt` read `$ENV{TEST_COMPONENTS}`, splitting on spaces if multi-valued. The FATAL_ERROR check becomes `if("$ENV{TEST_COMPONENTS}" STREQUAL "")`. The `REQUIRES unity ${TEST_COMPONENTS}` line becomes `REQUIRES unity ${_test_components}` after `string(REPLACE " " ";" _test_components "$ENV{TEST_COMPONENTS}")`.
>
> **C4 — Each test app's `EXTRA_COMPONENT_DIRS` must point at its specific test component dir, not the shared `test_components/` parent.** On chip targets (esp32p4) `set(COMPONENTS …)` is not used (per host-apps doc, that's linux-only), so every component discoverable via `EXTRA_COMPONENT_DIRS` is auto-included in the build closure. With `EXTRA_COMPONENT_DIRS = test_components/`, all of `test_a`/`test_b`/`test_int` get registered for every app — `test_a` then tries to `#include "Mockcomponent_B.h"` in an app where no mock injection runs, and the build fails with `fatal error: Mockcomponent_B.h: No such file or directory`.
>
> Fix: each test app points at exactly its own test component:
> ```cmake
> # test_apps/test_component_A/CMakeLists.txt
> set(EXTRA_COMPONENT_DIRS
>     "${CMAKE_CURRENT_LIST_DIR}/../../real_components"
>     "${CMAKE_CURRENT_LIST_DIR}/../../test_components/test_a"   # not test_components/
>     "${CMAKE_CURRENT_LIST_DIR}/../../test_common"
> )
> ```
> Same pattern for `test_b` and `test_int`. IDF treats an `EXTRA_COMPONENT_DIRS` entry whose dir contains a `CMakeLists.txt` as a single component (the dir basename becomes the component name), so this registers exactly one test component per app.
>
> **C5 — `pytest.ini` must declare `python_files` to match `pytest_test_apps.py`.** pytest's default file collection patterns are `test_*.py` and `*_test.py`. The spec ships the pytest entry point as `pytest_test_apps.py`, which matches neither. Without an override, `pytest -m unit` reports `collected 0 items / 0 selected` and silently exits success — a particularly nasty failure mode that fakes acceptance criterion #5.
>
> Fix: add `python_files = pytest_*.py test_*.py` to `[pytest]` in `pytest.ini`. (Alternatively rename the file to `test_apps.py`, but keeping the spec's filename and overriding the pattern is non-invasive and lets the file name signal "this is the pytest-embedded entry point.")
>
> **C7 — Per-component tests live nested in the component dir.** With C4's per-app `EXTRA_COMPONENT_DIRS` scoping (each app sees exactly one test component), the unique-test-dir-name constraint from the original spec only applies *across one binary*. Per-component tests can therefore live at `components/<comp>/test/` next to the code they test — naturally cohesive, mirrors esp-idf's own repo layout — and each registers as an IDF component named `test` without collision. Cross-component tests (e.g. `test_int`) have no natural home inside one component, so they stay in `test_components/<unique_name>/`. Test app CMakeLists list `components` plus `components/<comp>/test` (the second entry is what makes IDF's scanner pick up the nested `test/` dir as a separate component).
>
> **C6 — Per-target build dirs (`build_linux/`, `build_esp32p4/`).** The original spec used a single `build/` per app. `idf.py set-target X build` wipes the build dir on every target switch, so a workflow like "build esp32p4, then `pytest -m host`" silently runs an esp32p4 RISC-V `.elf` on the linux host — pexpect times out after 30s with empty buffer, no helpful error.
>
> Fix:
> - Build commands use `-B "$app/build_linux"` and `-B "$app/build_esp32p4"`.
> - `pytest_test_apps.py` parametrizes `build_dir` alongside `target`/`app_path`/`embedded_services`:
>   ```python
>   @pytest.mark.parametrize('embedded_services,target,app_path,build_dir', [
>       ('idf', 'linux', os.path.join(HERE, 'test_apps', 'test_component_A'), 'build_linux'),
>   ], indirect=True)
>   ```
> - `.gitignore` covers both `build_linux/` and `build_esp32p4/` (the existing `build_*/` pattern already does, but listing them explicitly is clearer).
> - CI workflow uses `-B "$app/build_linux"` for the linux job; the `espressif/esp-idf-ci-action` esp32p4 job's build dir is internal to that action.
>
> **Followup notes (not blocking, address during implementation):**
> - **F3** — Peripheral mocks (`mocks/peripheral_mocks/driver/`) should auto-copy into a test app's `components/` the same way `component_B`'s mock does, for consistency. Document the pattern in `mocks/peripheral_mocks/driver/README.md`.
> - **F4** — README must warn that `test_apps/*/components/` is regenerated each configure; hand edits there are clobbered.
> - **F5** — `mocks/component_A/` is provided as a template; not wired into any current test app. Note this in a comment so it isn't mistaken for unused dead code.
> - **F6** — Verify `--preview set-target linux` is still required on IDF v5.5.4. If not, drop the flag from CI and README. Keep the conditional shape.
> - **F7** — Confirm `pytest-embedded`'s `unity_tester` plumbing reports each `TEST_CASE` as a separate pytest result; bare `run_all_single_board_cases` may need the `unity` reporter fixture to satisfy acceptance criterion #5.
> - **F9** — CMock default mock prefix in `idf_component_mock` is `Mock` (no underscore), matching the test source's `#include "Mockcomponent_B.h"`. Don't override `:mock_prefix:` in `mock_config.yaml`.

## What changed vs. v2

v2 had one blocker plus polish issues. This revision fixes them:

1. **Fixed the empty-test-menu bug.** v2 listed test components in
   `set(COMPONENTS ...)` but nothing in the link graph required them, so
   they were built but their `TEST_CASE` linker sections were never pulled
   in. v3 makes `test_main` declare the per-app test components in
   `REQUIRES`, via a per-app `TEST_COMPONENTS` cache variable that
   `test_main`'s CMakeLists reads. The `WHOLE_ARCHIVE` on each test
   component then guarantees the sections survive the link.
2. **Removed `--embedded-services idf` from `pytest.ini` `addopts`.**
   It conflicted with the per-test parametrize. Each test now sets the
   service explicitly via `embedded_services` in its parametrize tuple.
3. **Mock `REQUIRES` cleaned up for consistency.** Both `component_A`
   and `component_B` mocks declare `REQUIRES esp_common`, matching the
   IDF docs example pattern.
4. **`driver` peripheral mock REQUIRES expanded** to include `hal` and
   `soc`. Even a single header from `driver/include/driver/` transitively
   pulls types from those components, so the stub now compiles
   out-of-the-box.
5. **Per-app mock is a CMake-time copy, not a checked-in duplicate.**
   The test app's CMakeLists copies `mocks/component_B/` into
   `${CMAKE_CURRENT_LIST_DIR}/components/component_B/` before
   `project()` is invoked. One source of truth, no symlinks, no
   Windows-runner pain, no manual sync drift.
6. **README documents the `[tag]` ↔ `group=` mapping** for
   `run_all_single_board_cases`.

All v2 corrections (override precedence, `COMPONENT_OVERRIDEN_DIR`,
`mock_config.yaml` mandatory, unique test-dir names, `set(COMPONENTS …)`
required for Linux, `--preview` only on `linux`) carry forward.

---

## Goal

Working ESP-IDF v5.5.4 project for the **ESP32-P4** target with two
pure-logic components (`component_A`, `component_B`), three test apps
(two unit, one integration), CMock mocking of `component_B` from
`component_A`'s unit test, and pytest-embedded orchestration. Unit
tests run on the Linux target only. The integration test runs on both
Linux and ESP32-P4.

## Constraints

- **IDF version**: `v5.5.4`. Refs:
  - https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-guides/unit-tests.html
  - https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-guides/host-apps.html
  - https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-guides/build-system.html
  - https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/contribute/esp-idf-tests-with-pytest.html
- **Targets**: `esp32p4` for hardware, `linux` for host tests.
- **Test framework**: Unity, IDF-style — `TEST_CASE` macros,
  `unity_run_menu()`.
- **Mocking**: CMock only, via `idf_component_mock()` from real headers.
- **Component dependency**: `component_A` calls `component_B`.
  `component_B` has no dependency on `component_A`.
- **Both components are pure logic.** May use `freertos`, `esp_event`,
  `esp_timer` (all supported on Linux per the host-apps support table).
- **Peripheral mocks scaffolded but minimal**:
  `mocks/peripheral_mocks/driver/` ships one mocked header so it
  compiles; README explains expansion.

## Final Directory Structure

```
my_project/
├── README.md
├── .gitignore
├── .github/workflows/test.yml
├── real_components/                   # added via EXTRA_COMPONENT_DIRS
│   ├── component_A/
│   │   ├── CMakeLists.txt
│   │   ├── include/component_A.h
│   │   ├── component_A.c
│   │   └── test_a/
│   │       ├── CMakeLists.txt
│   │       └── test_component_A.c
│   ├── component_B/
│   │   ├── CMakeLists.txt
│   │   ├── include/component_B.h
│   │   ├── component_B.c
│   │   └── test_b/
│   │       ├── CMakeLists.txt
│   │       └── test_component_B.c
│   └── integration_tests/
│       ├── CMakeLists.txt             # config-only parent
│       └── test_int/
│           ├── CMakeLists.txt
│           └── test_integration_AB.c
├── mocks/                             # canonical mock sources
│   ├── component_A/
│   │   ├── CMakeLists.txt
│   │   └── mock/mock_config.yaml
│   ├── component_B/
│   │   ├── CMakeLists.txt
│   │   └── mock/mock_config.yaml
│   └── peripheral_mocks/
│       └── driver/
│           ├── CMakeLists.txt
│           ├── mock/mock_config.yaml
│           └── README.md
├── test_common/
│   ├── sdkconfig.defaults
│   ├── sdkconfig.defaults.linux
│   ├── sdkconfig.defaults.esp32p4
│   └── test_main/
│       ├── CMakeLists.txt
│       └── test_main.c
├── test_apps/
│   ├── test_component_A/
│   │   └── CMakeLists.txt             # copies mock into components/ at configure time
│   ├── test_component_B/
│   │   └── CMakeLists.txt
│   └── test_integration_AB/
│       └── CMakeLists.txt
├── pytest.ini
├── conftest.py
├── pytest_test_apps.py
└── requirements.txt
```

**Override precedence (per build-system doc):** project `components/` >
`EXTRA_COMPONENT_DIRS` > managed > IDF built-in. Real components live
in `real_components/` (added via `EXTRA_COMPONENT_DIRS`) so the per-app
`components/` can shadow them with mocks.

---

## File Specifications

### Real components

**`real_components/component_A/include/component_A.h`**
```c
#pragma once
#include "esp_err.h"

esp_err_t component_A_init(void);
esp_err_t component_A_deinit(void);
esp_err_t component_A_do_work(int input, int *output);
```

**`real_components/component_A/component_A.c`**
```c
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
```

**`real_components/component_A/CMakeLists.txt`**
```cmake
idf_component_register(
    SRCS "component_A.c"
    INCLUDE_DIRS "include"
    REQUIRES component_B
)
```

**`real_components/component_B/include/component_B.h`**
```c
#pragma once
#include "esp_err.h"

esp_err_t component_B_init(void);
esp_err_t component_B_deinit(void);
int component_B_process(int input);
```

**`real_components/component_B/component_B.c`**
```c
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
```

**`real_components/component_B/CMakeLists.txt`**
```cmake
idf_component_register(
    SRCS "component_B.c"
    INCLUDE_DIRS "include"
)
```

### Per-component test directories

Unique directory names so the resulting test components have unique names
(no `test`/`test`/`test` collision when all three end up in the same build).

**`real_components/component_A/test_a/CMakeLists.txt`**
```cmake
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES unity component_A
    WHOLE_ARCHIVE
)
```

**`real_components/component_A/test_a/test_component_A.c`**
```c
#include "unity.h"
#include "component_A.h"
#include "Mockcomponent_B.h"   /* CMock-generated header — capital 'M' */

TEST_CASE("A init succeeds", "[component_A]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
    TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
}

TEST_CASE("A do_work delegates to B", "[component_A]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
    component_B_process_ExpectAndReturn(5, 10);
    int out = 0;
    TEST_ASSERT_EQUAL(ESP_OK, component_A_do_work(5, &out));
    TEST_ASSERT_EQUAL_INT(10, out);
    TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
}

TEST_CASE("A do_work fails on null output", "[component_A]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, component_A_do_work(5, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
}
```
CMock prefix is `Mock<header>.h` by default; if you change it in
`mock_config.yaml`, update the include.

**`real_components/component_B/test_b/CMakeLists.txt`**
```cmake
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES unity component_B
    WHOLE_ARCHIVE
)
```

**`real_components/component_B/test_b/test_component_B.c`**
```c
#include "unity.h"
#include "component_B.h"

TEST_CASE("B init succeeds", "[component_B]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_B_init());
    TEST_ASSERT_EQUAL(ESP_OK, component_B_deinit());
}
TEST_CASE("B process doubles input", "[component_B]")
{
    TEST_ASSERT_EQUAL_INT(14, component_B_process(7));
}
TEST_CASE("B process handles zero", "[component_B]")
{
    TEST_ASSERT_EQUAL_INT(0, component_B_process(0));
}
```

### Integration tests component

**`real_components/integration_tests/CMakeLists.txt`**
```cmake
# Config-only parent: idf_component_register with no SRCS.
idf_component_register(INCLUDE_DIRS "")
```

**`real_components/integration_tests/test_int/CMakeLists.txt`**
```cmake
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES unity component_A component_B
    WHOLE_ARCHIVE
)
```

**`real_components/integration_tests/test_int/test_integration_AB.c`**
```c
#include "unity.h"
#include "component_A.h"
#include "component_B.h"

TEST_CASE("A+B end-to-end: do_work returns doubled input", "[integration]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_B_init());
    TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
    int out = 0;
    TEST_ASSERT_EQUAL(ESP_OK, component_A_do_work(21, &out));
    TEST_ASSERT_EQUAL_INT(42, out);
    TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
    TEST_ASSERT_EQUAL(ESP_OK, component_B_deinit());
}

TEST_CASE("A+B init/deinit cycle", "[integration]")
{
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, component_B_init());
        TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
        TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
        TEST_ASSERT_EQUAL(ESP_OK, component_B_deinit());
    }
}
```

### Mock components (CMock)

Pattern: get the original component's directory via
`COMPONENT_OVERRIDEN_DIR` (note IDF's spelling — single 'D'), then call
`idf_component_mock()`. **No `include(idf.cmake)` line** — these files
are processed as components, not as standalone CMake projects.

**`mocks/component_B/CMakeLists.txt`**
```cmake
idf_component_get_property(original_dir component_B COMPONENT_OVERRIDEN_DIR)

idf_component_mock(
    INCLUDE_DIRS "${original_dir}/include"
    REQUIRES esp_common
    MOCK_HEADER_FILES "${original_dir}/include/component_B.h"
)
```

**`mocks/component_B/mock/mock_config.yaml`**
```yaml
:cmock:
  :plugins:
    - expect
    - expect_any_args
    - return_thru_ptr
    - ignore
    - ignore_arg
```

**`mocks/component_A/CMakeLists.txt`**
```cmake
idf_component_get_property(original_dir component_A COMPONENT_OVERRIDEN_DIR)

idf_component_mock(
    INCLUDE_DIRS "${original_dir}/include"
    REQUIRES esp_common
    MOCK_HEADER_FILES "${original_dir}/include/component_A.h"
)
```

**`mocks/component_A/mock/mock_config.yaml`** — identical to
`component_B`'s.

**`mocks/peripheral_mocks/driver/CMakeLists.txt`**
```cmake
# Stub mock for IDF `driver` component. Ships with gpio.h mocked so the
# directory builds. Add headers to MOCK_HEADER_FILES as needed.
#
# To use: copy or place this directory into a test app's components/
# (highest precedence) so it overrides IDF's built-in driver component.
idf_component_get_property(original_dir driver COMPONENT_OVERRIDEN_DIR)

idf_component_mock(
    INCLUDE_DIRS "${original_dir}/include"
    REQUIRES esp_common esp_hw_support hal soc
    MOCK_HEADER_FILES "${original_dir}/include/driver/gpio.h"
)
```
`hal` and `soc` are required because `driver/gpio.h` transitively
pulls types from `hal/gpio_types.h` and `soc/gpio_num.h`.

**`mocks/peripheral_mocks/driver/mock/mock_config.yaml`** — identical
to `component_B`'s.

**`mocks/peripheral_mocks/driver/README.md`**
````markdown
# Peripheral mock: `driver`

Shadows the ESP-IDF `driver` component when this directory sits inside
a test app's `components/` (highest precedence) or in a path scanned
*before* IDF's components via `EXTRA_COMPONENT_DIRS`.

## Adding more headers
Append to `MOCK_HEADER_FILES` in `CMakeLists.txt`:
```cmake
MOCK_HEADER_FILES
    "${original_dir}/include/driver/gpio.h"
    "${original_dir}/include/driver/i2c.h"
    "${original_dir}/include/driver/spi_master.h"
```
Headers may pull in additional component types — extend `REQUIRES`
as needed (e.g., `esp_pm` for power-management types).

## Adding mocks for other peripherals
Create a sibling directory whose name matches the IDF component to
shadow (e.g., `mocks/peripheral_mocks/esp_wifi/`). Mirror this
CMakeLists. **The directory name must match the original component
name exactly.**
````

### Shared sdkconfig

**`test_common/sdkconfig.defaults`**
```
CONFIG_COMPILER_OPTIMIZATION_DEBUG=y
```

**`test_common/sdkconfig.defaults.linux`**
```
# Linux uses FreeRTOS POSIX simulator. Keep this minimal — most
# chip-specific Kconfigs do not exist on Linux and will warn.
```

**`test_common/sdkconfig.defaults.esp32p4`**
```
CONFIG_ESP_TASK_WDT_INIT=n
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
```

### Shared test_main

`test_main` is the executable-side entry point. It declares its
`REQUIRES` from a per-app cache variable `TEST_COMPONENTS` so each test
app picks which test components get linked in. **This is what fixes the
v2 empty-test-menu bug**: without this `REQUIRES` edge from the
executable to the test components, their `WHOLE_ARCHIVE` is meaningless
because nothing links them.

**`test_common/test_main/CMakeLists.txt`**
```cmake
# TEST_COMPONENTS is set by each test app's root CMakeLists.txt before
# project() is invoked. It is a list of test-component names that this
# binary should pull in (e.g. "test_a", "test_b", "test_int").
if(NOT DEFINED TEST_COMPONENTS)
    message(FATAL_ERROR
        "TEST_COMPONENTS not set. Each test app's root CMakeLists must "
        "set(TEST_COMPONENTS test_a ...) before project().")
endif()

idf_component_register(
    SRCS "test_main.c"
    INCLUDE_DIRS "."
    REQUIRES unity ${TEST_COMPONENTS}
)
```

**`test_common/test_main/test_main.c`**
```c
#include "unity.h"

void app_main(void) {
    unity_run_menu();
}
```

### Test apps

Each app sets `TEST_COMPONENTS` before `project()`, then
`include($ENV{IDF_PATH}/tools/cmake/project.cmake)` picks it up when
processing `test_main`'s CMakeLists.

For `test_component_A`, the app also copies the canonical mock from
`mocks/component_B/` into its own `components/component_B/` at configure
time. The copy guarantees the per-app `components/` (highest precedence)
overrides the real `component_B` from `EXTRA_COMPONENT_DIRS`. Editing
`mocks/component_B/` and re-configuring is the supported workflow; no
manual sync.

**`test_apps/test_component_A/CMakeLists.txt`**
```cmake
cmake_minimum_required(VERSION 3.16)

# Copy canonical mock into this app's components/ (highest precedence)
# so it overrides the real component_B from real_components/.
file(REMOVE_RECURSE "${CMAKE_CURRENT_LIST_DIR}/components/component_B")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../mocks/component_B/"
     DESTINATION "${CMAKE_CURRENT_LIST_DIR}/components/component_B/")

set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults.${IDF_TARGET}"
)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../real_components"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common"
)

# Test components this binary must link in. Read by test_main's CMakeLists
# and turned into a REQUIRES edge so WHOLE_ARCHIVE actually pulls in
# TEST_CASE linker sections.
set(TEST_COMPONENTS test_a)

# On Linux, COMPONENTS must be set explicitly to disable auto-inclusion
# of all IDF components (per host-apps doc). REQUIRES expands the closure.
if("${IDF_TARGET}" STREQUAL "linux")
    set(COMPONENTS test_main component_A component_B test_a)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_component_A)
```

**`test_apps/test_component_B/CMakeLists.txt`** (no mocks needed):
```cmake
cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults.${IDF_TARGET}"
)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../real_components"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common"
)

set(TEST_COMPONENTS test_b)

if("${IDF_TARGET}" STREQUAL "linux")
    set(COMPONENTS test_main component_B test_b)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_component_B)
```

**`test_apps/test_integration_AB/CMakeLists.txt`** (real components):
```cmake
cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults.${IDF_TARGET}"
)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../real_components"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common"
)

set(TEST_COMPONENTS test_int)

if("${IDF_TARGET}" STREQUAL "linux")
    set(COMPONENTS test_main component_A component_B integration_tests test_int)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_integration_AB)
```

### pytest infrastructure

**`requirements.txt`**
```
pytest>=8,<9
pytest-embedded>=1.10,<2
pytest-embedded-idf>=1.10,<2
pytest-embedded-serial-esp>=1.10,<2
```

**`pytest.ini`**
```ini
[pytest]
addopts = -ra -v
markers =
    host: tests that run on the IDF Linux target
    esp32p4: tests that run on ESP32-P4 hardware
    unit: unit tests (with mocks)
    integration: integration tests (real components)
```
`embedded_services` is **not** in `addopts` — each test sets it via
parametrize, so the linux runs use `idf` only and the esp32p4 run uses
`esp,idf` without conflict.

**`conftest.py`**
```python
# pytest-embedded plugins are loaded automatically via their entry points
# on `pip install pytest-embedded-idf`. Add custom fixtures here if needed.
```

**`pytest_test_apps.py`**
```python
import os
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))


# ---------- Unit tests (Linux only, mocks active) ----------

@pytest.mark.unit
@pytest.mark.host
@pytest.mark.parametrize('embedded_services,target,app_path', [
    ('idf', 'linux', os.path.join(HERE, 'test_apps', 'test_component_A')),
], indirect=True)
def test_unit_component_A(dut):
    # group= matches the Unity tag inside [brackets] in TEST_CASE.
    dut.run_all_single_board_cases(group='component_A')


@pytest.mark.unit
@pytest.mark.host
@pytest.mark.parametrize('embedded_services,target,app_path', [
    ('idf', 'linux', os.path.join(HERE, 'test_apps', 'test_component_B')),
], indirect=True)
def test_unit_component_B(dut):
    dut.run_all_single_board_cases(group='component_B')


# ---------- Integration tests ----------

@pytest.mark.integration
@pytest.mark.host
@pytest.mark.parametrize('embedded_services,target,app_path', [
    ('idf', 'linux', os.path.join(HERE, 'test_apps', 'test_integration_AB')),
], indirect=True)
def test_integration_linux(dut):
    dut.run_all_single_board_cases(group='integration')


@pytest.mark.integration
@pytest.mark.esp32p4
@pytest.mark.parametrize('embedded_services,target,app_path', [
    ('esp,idf', 'esp32p4', os.path.join(HERE, 'test_apps', 'test_integration_AB')),
], indirect=True)
def test_integration_esp32p4(dut):
    dut.run_all_single_board_cases(group='integration')
```

### .gitignore

```
build/
build_*/
sdkconfig
sdkconfig.old
managed_components/
dependencies.lock
__pycache__/
*.pyc
.pytest_cache/
results-*.xml

# Per-app mock copies — regenerated at CMake configure time
test_apps/*/components/
```

### README.md

Top-level README must include:
- One-paragraph project overview.
- Prerequisites: ESP-IDF v5.5.4 installed and exported, Python 3.10+,
  `pip install -r requirements.txt`, Ruby (CMock).
- Build commands per target (snippet below).
- Run commands: `pytest -m unit`, `pytest -m "integration and host"`,
  `pytest -m esp32p4`.
- Directory tree with one-line annotations.
- **"How tests are discovered"** section explaining: tag inside
  `TEST_CASE("name", "[tag]")` is what `run_all_single_board_cases(group=...)`
  matches. Group names in `pytest_test_apps.py` must equal the bracket
  tag verbatim.
- "Adding a new component test" — 6 numbered steps.
- "Adding a peripheral mock" — pointer to
  `mocks/peripheral_mocks/driver/README.md`.
- Note that `--preview` is needed only for the `linux` target.
- Note that `test_apps/*/components/` is git-ignored: the per-app mock
  copy is regenerated each `cmake` configure from `mocks/`.

### CI: `.github/workflows/test.yml`

Two jobs.

**`host-tests`** (on `ubuntu-22.04`):
- Set up Python 3.10, install IDF v5.5.4 via the official install
  scripts.
- `pip install -r requirements.txt`.
- Build all three test apps for `linux`:
  ```bash
  for app in test_apps/*/; do
      idf.py -C "$app" -B "$app/build" --preview set-target linux build
  done
  ```
- `pytest -m host --junitxml=results-host.xml`
- Upload `results-host.xml`.

**`target-build`** (build-only on `ubuntu-22.04`):
- `espressif/esp-idf-ci-action@v1` with `target: esp32p4`.
- Build only `test_apps/test_integration_AB`.
- Workflow comment: hardware execution (`pytest -m esp32p4`) requires a
  self-hosted runner with a connected ESP32-P4.

---

## Build & Verification Commands

```bash
# Setup
. $IDF_PATH/export.sh
pip install -r requirements.txt

# Build all three test apps for Linux (--preview required for linux only)
for app in test_apps/*/; do
    idf.py -C "$app" -B "$app/build" --preview set-target linux build
done

# Build integration app for ESP32-P4
idf.py -C test_apps/test_integration_AB set-target esp32p4 build

# Run host tests
pytest -m host

# Run hardware tests (requires connected ESP32-P4)
idf.py -C test_apps/test_integration_AB flash
pytest -m esp32p4
```

---

## Acceptance Criteria

1. `idf.py -C test_apps/test_component_A --preview set-target linux build`
   succeeds. **Verify mock is linked:** grep build log for
   `Mockcomponent_B.c` compilation. Real `component_B.c` should NOT
   appear.
2. `idf.py -C test_apps/test_component_B --preview set-target linux build`
   succeeds.
3. `idf.py -C test_apps/test_integration_AB --preview set-target linux build`
   succeeds with both real components linked.
4. `idf.py -C test_apps/test_integration_AB set-target esp32p4 build`
   succeeds.
5. **`pytest -m unit` actually runs the test cases (not zero).** Each
   `TEST_CASE` is reported as a separate pytest result via
   `run_all_single_board_cases(group=...)`. If pytest reports "0 tests
   collected" or "menu was empty", the `TEST_COMPONENTS`/`REQUIRES`
   wiring is broken — re-check `test_common/test_main/CMakeLists.txt`.
6. `pytest -m "integration and host"` runs the Linux integration test.
7. `mocks/peripheral_mocks/driver/` builds when copied into a test
   app's `components/`. Shipped stub mocks `driver/gpio.h`.
8. Adding a header to `MOCK_HEADER_FILES` in
   `mocks/peripheral_mocks/driver/CMakeLists.txt` is sufficient to mock
   it (modulo extra `REQUIRES` if the header pulls types from new
   components).

---

## Notes for Claude Code

- **The empty-test-menu trap (the v2 bug):** Listing test components
  in `set(COMPONENTS …)` is necessary on Linux but not sufficient —
  there must also be a `REQUIRES` edge from the executable's main
  component to each test component. v3 routes this through
  `TEST_COMPONENTS` → `test_main`'s `REQUIRES`. Don't break that
  chain.
- **Override precedence is real**: project `components/` >
  `EXTRA_COMPONENT_DIRS` > managed > IDF. Real components live in
  `real_components/` (via `EXTRA_COMPONENT_DIRS`) so the per-app
  `components/` can shadow them.
- **`COMPONENT_OVERRIDEN_DIR`** (single 'D' — IDF's spelling) is the
  documented way to find the original component when writing a mock.
  Don't hardcode `$ENV{IDF_PATH}/components/...` paths.
- **`mock_config.yaml` is mandatory.** Without `:plugins: [expect]`,
  `_ExpectAndReturn` symbols won't be generated and tests won't link.
- **Test directory naming matters.** Three components all named `test`
  collide. Use `test_a`, `test_b`, `test_int`.
- **`WHOLE_ARCHIVE` on test components** keeps `TEST_CASE` linker
  sections from being stripped — but only matters once the component
  is actually linked (see the v2 bug note above).
- **`--preview` on `set-target`** is required for `linux` only; not
  for `esp32p4` or other shipping chips.
- **`pytest-embedded` services**: don't put `--embedded-services` in
  `pytest.ini` `addopts` — it conflicts with per-test parametrize.
- **Per-app mock copy is automatic.** `file(COPY ...)` runs each
  configure. To edit the mock: edit `mocks/component_B/`, then re-run
  `idf.py … reconfigure` (or just `build`). The copy under
  `test_apps/*/components/` is git-ignored.
- **After scaffolding, run all four build commands above and fix
  errors before declaring done.** Then run `pytest -m unit` and
  confirm test count > 0 — that's the only way to catch the v2-style
  empty-menu regression.
