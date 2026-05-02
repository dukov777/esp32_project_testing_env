# Stub mock for IDF `driver` component. Ships with gpio.h mocked so the
# directory builds. Add headers to MOCK_HEADER_FILES as needed.
#
# To use: have the test app's root CMakeLists.txt copy this directory into
# components/driver/ before project() (mirroring the component_B pattern).
idf_component_get_property(original_dir driver COMPONENT_OVERRIDEN_DIR)

idf_component_mock(
    INCLUDE_DIRS "${original_dir}/include"
    REQUIRES esp_common esp_hw_support hal soc
    MOCK_HEADER_FILES "${original_dir}/include/driver/gpio.h"
)
