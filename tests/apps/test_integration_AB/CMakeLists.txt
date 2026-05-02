cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../common/sdkconfig.defaults"
)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../../components"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/test_int"
    "${CMAKE_CURRENT_LIST_DIR}/../../common"
)

set(ENV{TEST_COMPONENTS} "test_int")

if("${IDF_TARGET}" STREQUAL "linux")
    set(COMPONENTS test_main component_A component_B test_int)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_integration_AB)
