# Scaffold spec — historical corrections ledger

> **Status:** the original v3 scaffold body has been removed because it
> referenced directory names (`real_components/`, `test_apps/`,
> `test_components/`, `test_common/`) and CMake patterns that have since
> been corrected and superseded by the live tree. Copying snippets from
> the old body would re-introduce the very bugs C1–C8 below were filed
> to fix.
>
> **What remains** (below) is the v3.1 corrections prelude — the design
> rationale for why the current invariants exist. It is the failure-log
> companion to `CLAUDE.md`: `CLAUDE.md` documents the *rules* that the
> live tree obeys; this file records *what broke* before those rules
> existed.
>
> **For current authoritative state** (directory layout, CMake idioms,
> commands), see `CLAUDE.md` and `README.md`. Do not treat anything
> below as a coding template.

---

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
> **C8 — All test infrastructure consolidated under `tests/`.** Original spec had `mocks/`, `test_common/`, `test_components/`, `test_apps/` as four sibling top-level directories alongside `real_components/`. Refactor moves all four under a single `tests/` umbrella:
> ```
> tests/
> ├── apps/         (was test_apps/)
> ├── common/       (was test_common/)
> ├── components/   (was test_components/ — cross-component tests)
> └── mocks/        (was mocks/)
> ```
> Production `components/` stays at repo root — it isn't test-only. Two `components/` dirs now exist (`/components/` for production, `/tests/components/` for cross-component tests); paths are unambiguous.
>
> Effects on `EXTRA_COMPONENT_DIRS`: from `tests/apps/<X>/`, paths to siblings under `tests/` (`../../mocks`, `../../common`) keep their old `../../` form; paths up to the production `components/` deepen by one (`../../../components`). Scripts (`build-all.sh`, `clean-all.sh`), CI (`.github/workflows/test.yml`), `.gitignore`, `pytest_test_apps.py`, README, and CLAUDE.md updated to match.
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
