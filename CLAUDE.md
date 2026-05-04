# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Status

Project scaffolded per `SCAFFOLD_SPEC.md` (v3 + v3.1 corrections prelude). Live tree:

- **`/components/`** — production components. Each has its unit tests nested at `components/<comp>/test/` (per C7 — tests live next to the code they exercise).
- **`/tests/`** — umbrella for all test infra (per C8):
  - `apps/` — one IDF project per test binary
  - `common/` — shared `app_main` + sdkconfig defaults
  - `components/` — cross-component tests (currently just `test_int`)
  - `mocks/` — canonical CMock sources. `mocks/peripheral_mocks/driver/` is a template stub for mocking IDF's built-in `driver` component (e.g. `gpio.h`); copy it like the `component_B` pattern when a test app needs to fake hardware peripherals.

`SCAFFOLD_SPEC.md` is kept for traceability; the prelude (corrections C1–C8) is the authoritative ledger of what changed and why.

## Project Purpose

ESP-IDF v5.5.4 reference project demonstrating a working unit + integration test setup with CMock for the **ESP32-P4** target, plus host-side (`linux` target) execution of the same Unity test cases via the IDF FreeRTOS POSIX simulator.

## Toolchain & Targets

- **ESP-IDF**: `v5.5.4` (no other version). Always `. $IDF_PATH/export.sh` before any `idf.py` command. Stale `tests/apps/*/sdkconfig` (gitignored) from a prior IDF version will be regenerated on next configure — if you copy a live tree elsewhere, delete the per-app `sdkconfig` first so the new IDF writes a fresh one.
- **Hardware target**: `esp32p4`.
- **Host target**: `linux` (requires `--preview` on `set-target`; not needed for `esp32p4`).
- **Test framework**: Unity, IDF-style — `TEST_CASE(...)` macros + `unity_run_menu()`.
- **Mocking**: CMock only, via `idf_component_mock()`. Ruby is required at build time.
- **Test orchestration**: `pytest-embedded` + `pytest-embedded-idf` (and `-serial-esp` for hardware).

## Architecture (the non-obvious bits)

The interesting part of this project is **how the override + mock + link wiring fits together**. Reading any single CMakeLists in isolation will not explain it. Key invariants:

1. **Override precedence**: a test app's own `<test_app>/components/` (highest) > `EXTRA_COMPONENT_DIRS` > managed > IDF built-in. The repo-root `components/` directory is added to each test app via `EXTRA_COMPONENT_DIRS`, so each test app's local `<test_app>/components/` (the auto-injected mock copy) can shadow it.
2. **Per-app mock injection is a CMake-time copy**, not a checked-in duplicate. Test apps that mock a dependency do `file(COPY ${CMAKE_CURRENT_LIST_DIR}/../../mocks/<name>/ DESTINATION components/<name>/)` *before* `project()` (preceded by `file(REMOVE_RECURSE …)` to avoid stale copies). Currently only `test_component_A` injects a mock (for `component_B`); `test_component_B` and `test_integration_AB` link the real components. `tests/apps/*/components/` is git-ignored — edit the canonical source under `tests/mocks/`, then reconfigure.
3. **`COMPONENT_OVERRIDEN_DIR`** (single 'D' — IDF's spelling) is the only correct way for a mock CMakeLists to locate the real component's headers. Do not hardcode `$ENV{IDF_PATH}/components/...`.
4. **`mock_config.yaml` is mandatory.** Without `:plugins: [expect, ...]`, `_ExpectAndReturn` symbols are not generated and tests fail to link.
5. **The empty-test-menu trap.** `WHOLE_ARCHIVE` on a test component is meaningless if nothing links it. The fix routes each app's test components through a `TEST_COMPONENTS` env var (see invariant 8 for why env var, not CMake variable) that `tests/common/test_main/CMakeLists.txt` reads and turns into a `REQUIRES` edge. Breaking that chain produces a binary that builds fine but reports zero tests at runtime.
6. **On the `linux` target, `set(COMPONENTS …)` must be set explicitly** in each test app to disable auto-inclusion of every IDF component (per host-apps doc). On `esp32p4` it is omitted.
7. **Per-component tests live next to the code** (`components/<comp>/test/`); cross-component tests live in `tests/components/<unique_name>/` (currently just `test_int`). Each component's `test/` subdir is registered as an IDF component named `test` — collisions are avoided because each test app's `EXTRA_COMPONENT_DIRS` (per invariant 9) only points at one such dir, so any single binary contains at most one component named `test`. The `test_int` cross-component test gets a unique basename because it has no natural home inside one component.
8. **`TEST_COMPONENTS` is passed via env var, not CMake variable.** Each test app does `set(ENV{TEST_COMPONENTS} "test")` (unit-test apps, picking up the nested `components/<comp>/test/` component) or `"test_int"` (integration app) before `project()`; `tests/common/test_main/CMakeLists.txt` reads `$ENV{TEST_COMPONENTS}`. CMake variables don't survive IDF's `cmake -P` requirements pre-scan subprocess; env vars do.
9. **Each test app's `EXTRA_COMPONENT_DIRS` points at its specific test component dir** (e.g. `components/component_A/test` for the unit-A app, `tests/components/test_int` for the integration app), not at a parent that contains multiple test components. On chip targets `set(COMPONENTS …)` isn't used, so listing a multi-test parent would auto-include every test in every app and cross-app mock includes wouldn't resolve.
10. **Per-target build dirs (`build_linux/`, `build_esp32p4/`).** `idf.py set-target X build` wipes the build dir on every switch, so a shared `build/` lets a stale RISC-V binary leak into a host pytest run; per-target dirs eliminate the footgun. `pytest_test_apps.py` parametrizes `build_dir` alongside `target`/`embedded_services`.
11. **`pytest.ini` overrides `python_files`** to `pytest_*.py test_*.py` so `pytest_test_apps.py` is collected (default would match neither, leading to a silent zero-tests pass).

## Commands

```bash
# Setup (every shell)
. $IDF_PATH/export.sh
pip install -r requirements.txt

# Build all test apps for the host (Linux) target — uses build_linux/
for app in tests/apps/*/; do
    idf.py -C "$app" -B "$app/build_linux" --preview set-target linux build
done

# Build the integration app for ESP32-P4 hardware — uses build_esp32p4/
idf.py -C tests/apps/test_integration_AB \
    -B tests/apps/test_integration_AB/build_esp32p4 \
    set-target esp32p4 build

# Run host (Linux-target) tests
pytest -m host

# Run only unit tests (host, with mocks)
pytest -m unit

# Run only the host integration test
pytest -m "integration and host"

# Run a single test
pytest pytest_test_apps.py::test_unit_component_A

# Hardware tests (requires connected ESP32-P4) — keep build dir consistent
# with the build step above so pytest finds the right artifacts.
idf.py -C tests/apps/test_integration_AB \
    -B tests/apps/test_integration_AB/build_esp32p4 flash
pytest -m esp32p4
```

## Test Discovery (tag ↔ group mapping)

The string in brackets in `TEST_CASE("name", "[tag]")` is what `dut.run_all_single_board_cases(group=...)` matches. The `group=` argument in `pytest_test_apps.py` must equal the bracket tag verbatim — change one without the other and the test silently runs zero cases.

## pytest-embedded gotcha

Do **not** put `--embedded-services …` in `pytest.ini` `addopts`. Each test sets `embedded_services` in its parametrize tuple (`'idf'` for linux, `'esp,idf'` for esp32p4). The two forms conflict and silently break service selection.

## When verifying a build

The most common regression in this project is the empty-test-menu bug. Don't trust a green build alone:
- Confirm mocks are actually linked: build log should show `Mockcomponent_B.c` compiled and the real `component_B.c` should *not* appear in `tests/apps/test_component_A`.
- Run `pytest -m unit` and verify the test count is non-zero. Zero collected = `TEST_COMPONENTS`/`REQUIRES` chain is broken.
