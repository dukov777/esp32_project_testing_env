# Template mock for component_A. NOT wired into any current test app —
# v3.1 only mocks component_B (used by component_A's unit test). This
# directory is provided as a template if a future test app needs to mock A.
idf_component_get_property(original_dir component_A COMPONENT_OVERRIDEN_DIR)

idf_component_mock(
    INCLUDE_DIRS "${original_dir}/include"
    REQUIRES esp_common
    MOCK_HEADER_FILES "${original_dir}/include/component_A.h"
)
