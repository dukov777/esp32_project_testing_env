import os
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))


# ---------- Unit tests (Linux only, mocks active) ----------

# Build dirs are per-target so esp32p4 and linux builds don't clobber each
# other (`idf.py set-target X build` wipes the build dir; using one shared
# `build/` makes `pytest -m host` silently run a stale RISC-V binary).
BUILD_LINUX = 'build_linux'
BUILD_ESP32P4 = 'build_esp32p4'


@pytest.mark.unit
@pytest.mark.host
@pytest.mark.parametrize('embedded_services,target,app_path,build_dir', [
    ('idf', 'linux', os.path.join(HERE, 'test_apps', 'test_component_A'), BUILD_LINUX),
], indirect=True)
def test_unit_component_A(dut):
    # group= matches the Unity tag inside [brackets] in TEST_CASE.
    dut.run_all_single_board_cases(group='component_A')


@pytest.mark.unit
@pytest.mark.host
@pytest.mark.parametrize('embedded_services,target,app_path,build_dir', [
    ('idf', 'linux', os.path.join(HERE, 'test_apps', 'test_component_B'), BUILD_LINUX),
], indirect=True)
def test_unit_component_B(dut):
    dut.run_all_single_board_cases(group='component_B')


# ---------- Integration tests ----------

@pytest.mark.integration
@pytest.mark.host
@pytest.mark.parametrize('embedded_services,target,app_path,build_dir', [
    ('idf', 'linux', os.path.join(HERE, 'test_apps', 'test_integration_AB'), BUILD_LINUX),
], indirect=True)
def test_integration_linux(dut):
    dut.run_all_single_board_cases(group='integration')


@pytest.mark.integration
@pytest.mark.esp32p4
@pytest.mark.parametrize('embedded_services,target,app_path,build_dir', [
    ('esp,idf', 'esp32p4', os.path.join(HERE, 'test_apps', 'test_integration_AB'), BUILD_ESP32P4),
], indirect=True)
def test_integration_esp32p4(dut):
    dut.run_all_single_board_cases(group='integration')
