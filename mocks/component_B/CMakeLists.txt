idf_component_get_property(original_dir component_B COMPONENT_OVERRIDEN_DIR)

idf_component_mock(
    INCLUDE_DIRS "${original_dir}/include"
    REQUIRES esp_common
    MOCK_HEADER_FILES "${original_dir}/include/component_B.h"
)
