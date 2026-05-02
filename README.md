# ESP-IDF Test Project (esp32p4 + linux host)

[![tests](https://github.com/dukov777/esp32_project_testing_env/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/dukov777/esp32_project_testing_env/actions/workflows/test.yml)

Reference project demonstrating ESP-IDF v5.5.4 unit + integration tests
with CMock mocking, runnable on the `linux` host target (FreeRTOS POSIX
simulator) and on ESP32-P4 hardware. Two pure-logic components
(`component_A` calls `component_B`), three test apps (two unit, one
integration), and pytest-embedded orchestration.

## Prerequisites

- ESP-IDF `v5.5.4` installed and `. $IDF_PATH/export.sh` run in your shell.
- Python 3.10+.
- `pip install -r requirements.txt`.
- Ruby (required by CMock for mock generation).

## Layout

```
.
├── components/             # production components (added via EXTRA_COMPONENT_DIRS)
│   ├── component_A/
│   │   ├── include/, component_A.c, CMakeLists.txt
│   │   └── test/           # unit tests for component_A (mocks B)
│   └── component_B/
│       ├── include/, component_B.c, CMakeLists.txt
│       └── test/           # unit tests for component_B
├── test_components/        # cross-component tests (no single owner)
│   └── test_int/           # integration tests for A+B
├── mocks/                  # canonical mock sources (CMock)
│   ├── component_A/        # template (not currently wired into any app)
│   ├── component_B/        # used by test_apps/test_component_A
│   └── peripheral_mocks/
│       └── driver/         # IDF `driver` stub mock; see its README
├── test_common/
│   ├── sdkconfig.defaults{,.linux,.esp32p4}
│   └── test_main/          # shared app_main calling unity_run_menu()
├── test_apps/              # one IDF project per test binary
│   ├── test_component_A/
│   ├── test_component_B/
│   └── test_integration_AB/
├── pytest.ini
├── conftest.py
├── pytest_test_apps.py
└── requirements.txt
```

## Build

```bash
. $IDF_PATH/export.sh
pip install -r requirements.txt

# Build all test apps for the host (linux) target — uses build_linux/
for app in test_apps/*/; do
    idf.py -C "$app" -B "$app/build_linux" --preview set-target linux build
done

# Build the integration app for ESP32-P4 hardware — uses build_esp32p4/
idf.py -C test_apps/test_integration_AB -B test_apps/test_integration_AB/build_esp32p4 \
    set-target esp32p4 build
```

Per-target build dirs (`build_linux/`, `build_esp32p4/`) are deliberate.
`idf.py set-target X build` wipes the build dir on each target switch;
sharing one `build/` between targets means a later esp32p4 build silently
overwrites the linux `.elf`, and `pytest -m host` then tries to launch a
RISC-V binary on the host — pexpect times out with no output.

`--preview` is required only on the `linux` target; not for `esp32p4` or
other shipping chips.

## Run tests

```bash
pytest -m unit                       # unit tests on linux (with mocks)
pytest -m "integration and host"     # integration test on linux
pytest -m esp32p4                    # integration test on hardware (after flash)
pytest pytest_test_apps.py::test_unit_component_A   # single test
```

Hardware run requires:

```bash
idf.py -C test_apps/test_integration_AB flash
pytest -m esp32p4
```

## How tests are discovered

Each Unity case carries a tag in brackets, e.g.:

```c
TEST_CASE("A do_work delegates to B", "[component_A]")
```

`pytest_test_apps.py` invokes `dut.run_all_single_board_cases(group=...)`
with `group=` set to the **same string that appears between the
brackets**. `group='component_A'` runs every `TEST_CASE` tagged
`[component_A]`. **The strings must match verbatim** — change one without
the other and the test silently runs zero cases.

## Adding a new component test

1. Create `components/<comp>/{include/<comp>.h, <comp>.c, CMakeLists.txt}`.
2. Create `components/<comp>/test/{CMakeLists.txt, test_<comp>.c}` with
   `WHOLE_ARCHIVE` and `REQUIRES unity <comp>`. Test sources live next
   to the code they test.
3. Tag each `TEST_CASE` with a unique bracket string (e.g. `[<comp>]`).
4. If mocking another component, add `mocks/<other>/{CMakeLists.txt, mock/mock_config.yaml}`.
5. Create `test_apps/test_<comp>/CMakeLists.txt` mirroring the existing
   apps. The test app's `EXTRA_COMPONENT_DIRS` must list both
   `components` and `components/<comp>/test` (the second entry is what
   makes the nested test dir discoverable as a separate component named
   `test`). Copy any mock into `components/<other>/` before `project()`,
   `set(ENV{TEST_COMPONENTS} "test")`, and on linux
   `set(COMPONENTS test_main <comp> ... test)`.
6. Add a `test_unit_<comp>` function to `pytest_test_apps.py` with the
   right `group=` value.

For cross-component tests (e.g. integration of A + B), put them under
`test_components/<unique_name>/` instead — no single component owns them.

## Adding a peripheral mock

See `mocks/peripheral_mocks/driver/README.md`. New peripherals get a
sibling directory whose name matches the IDF component to shadow.

## Edits to mocks

The canonical mock sources live under `mocks/`. Each test app's
`CMakeLists.txt` runs `file(COPY mocks/<x>/ DESTINATION components/<x>/)`
at configure time. **`test_apps/*/components/` is git-ignored and
regenerated each configure — do not hand-edit, your changes will be
silently clobbered on the next `idf.py … reconfigure`.**

## CI

`.github/workflows/test.yml` runs:
- `host-tests` — builds all three test apps for `linux` and runs
  `pytest -m host`.
- `target-build` — build-only for `esp32p4` (integration app). Hardware
  execution (`pytest -m esp32p4`) requires a self-hosted runner with a
  connected ESP32-P4.

The badge at the top of this README reflects the latest `main` run.

### Enforce on PRs (branch protection)

To block merges to `main` until CI passes:

1. GitHub repo → **Settings** → **Branches** → **Add branch ruleset**
   (or "Add rule" on older UI) targeting `main`.
2. Enable **Require status checks to pass before merging**.
3. Add `host-tests` and `target-build` to the required-checks list.
4. Optionally enable **Require branches to be up to date before merging**
   so PRs always run CI against the current `main` tip.

Once configured, the PR's "Merge" button stays disabled until both jobs
report green.
