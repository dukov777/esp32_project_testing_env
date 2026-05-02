#include "unity.h"
#include "component_A.h"
#include "Mockcomponent_B.h"

TEST_CASE("A init succeeds", "[component_A]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
    TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
}

TEST_CASE("A do_work delegates to B", "[component_A]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
    component_B_process_ExpectAndReturn(5, 10);
    int out = 0;
    TEST_ASSERT_EQUAL(ESP_OK, component_A_do_work(5, &out));
    TEST_ASSERT_EQUAL_INT(10, out);
    TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
}

TEST_CASE("A do_work fails on null output", "[component_A]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, component_A_do_work(5, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
}
