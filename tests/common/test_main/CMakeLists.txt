# TEST_COMPONENTS is passed in via the TEST_COMPONENTS environment
# variable, set by each test app's root CMakeLists.txt before project().
# Env vars (not CMake variables) are used because IDF's component
# requirements pre-scan runs as a separate `cmake -P` subprocess and
# does not inherit the parent CMakeLists's variable scope — but it does
# inherit environment variables.
if("$ENV{TEST_COMPONENTS}" STREQUAL "")
    message(FATAL_ERROR
        "TEST_COMPONENTS env var not set. Each test app's root CMakeLists "
        "must set(ENV{TEST_COMPONENTS} \"test\") (unit-test apps picking up "
        "the nested components/<comp>/test/ component) or "
        "set(ENV{TEST_COMPONENTS} \"test_int\") (integration app) before "
        "project().")
endif()

# Convert space-separated env string into a CMake list.
string(REPLACE " " ";" _test_components "$ENV{TEST_COMPONENTS}")

idf_component_register(
    SRCS "test_main.c"
    INCLUDE_DIRS "."
    REQUIRES unity ${_test_components}
)
