#include "unity.h"
#include "component_B.h"

TEST_CASE("B init succeeds", "[component_B]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_B_init());
    TEST_ASSERT_EQUAL(ESP_OK, component_B_deinit());
}
TEST_CASE("B process doubles input", "[component_B]")
{
    TEST_ASSERT_EQUAL_INT(14, component_B_process(7));
}
TEST_CASE("B process handles zero", "[component_B]")
{
    TEST_ASSERT_EQUAL_INT(0, component_B_process(0));
}
