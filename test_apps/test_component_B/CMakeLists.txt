cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common/sdkconfig.defaults.${IDF_TARGET}"
)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../real_components"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_components/test_b"
    "${CMAKE_CURRENT_LIST_DIR}/../../test_common"
)

set(ENV{TEST_COMPONENTS} "test_b")

if("${IDF_TARGET}" STREQUAL "linux")
    set(COMPONENTS test_main component_B test_b)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_component_B)
