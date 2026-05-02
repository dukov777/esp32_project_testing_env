#include "unity.h"
#include "component_A.h"
#include "component_B.h"

TEST_CASE("A+B end-to-end: do_work returns doubled input", "[integration]")
{
    TEST_ASSERT_EQUAL(ESP_OK, component_B_init());
    TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
    int out = 0;
    TEST_ASSERT_EQUAL(ESP_OK, component_A_do_work(21, &out));
    TEST_ASSERT_EQUAL_INT(42, out);
    TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
    TEST_ASSERT_EQUAL(ESP_OK, component_B_deinit());
}

TEST_CASE("A+B init/deinit cycle", "[integration]")
{
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, component_B_init());
        TEST_ASSERT_EQUAL(ESP_OK, component_A_init());
        TEST_ASSERT_EQUAL(ESP_OK, component_A_deinit());
        TEST_ASSERT_EQUAL(ESP_OK, component_B_deinit());
    }
}
