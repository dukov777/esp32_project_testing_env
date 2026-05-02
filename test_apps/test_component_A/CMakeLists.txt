cmake_minimum_required(VERSION 3.16)

# Copy canonical mock into this app's components/ (highest precedence)
# so it overrides the real component_B from real_components/.
file(REMOVE_RECURSE "${CMAKE_CURRENT_LIST_DIR}/components/component_B")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../mocks/component_B/"
     DESTINATION "${CMAKE_CURRENT_LIST_DIR}/components/component_B/")

# IDF auto-discovers sdkconfig.defaults.<target> alongside the base file —
# don't list the suffixed path explicitly (IDF_TARGET is undefined here
# before project(), and CI actions like espressif/esp-idf-ci-action don't
# run `set-target` so the variable expands to empty → CMake error).
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults"
)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/component_A/test"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common"
)

# Test components this binary must link in. Read by test_main's CMakeLists
# (via the env var, which propagates into IDF's `cmake -P` requirements
# pre-scan subprocess; a regular CMake variable would not). Turned into a
# REQUIRES edge so WHOLE_ARCHIVE actually pulls in TEST_CASE linker sections.
set(ENV{TEST_COMPONENTS} "test")

# On Linux, COMPONENTS must be set explicitly to disable auto-inclusion
# of all IDF components (per host-apps doc). REQUIRES expands the closure.
if("${IDF_TARGET}" STREQUAL "linux")
    set(COMPONENTS test_main component_A component_B test)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_component_A)
