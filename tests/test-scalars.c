#include "test-suite.h"

void testScalars_zeroInitIsNull(void)
{
    AVar value = {0};

    TEST_ASSERT_EQUAL_HEX32(A_NULL, aVar_type(&value));
    TEST_ASSERT_FALSE(aVar_asBool(&value));
}

void testScalars_roundTripNumbers(void)
{
    AVar value = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setBool(&value, true));
    TEST_ASSERT_TRUE(aVar_asBool(&value));
    TEST_ASSERT_BITS_HIGH(A_FLAG_BOOL_VAL, value.type);

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&value, -42));
    TEST_ASSERT_EQUAL_HEX32(A_INT64, aVar_type(&value));
    TEST_ASSERT_EQUAL_INT64(-42, aVar_asI64(&value));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setU64(&value, 99u));
    TEST_ASSERT_EQUAL_HEX32(A_UINT64, aVar_type(&value));
    TEST_ASSERT_EQUAL_UINT64(99u, aVar_asU64(&value));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&value, -7));
    TEST_ASSERT_EQUAL_HEX32(A_INT32, aVar_type(&value));
    TEST_ASSERT_EQUAL_INT32(-7, aVar_asI32(&value));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setU32(&value, 1234u));
    TEST_ASSERT_EQUAL_HEX32(A_UINT32, aVar_type(&value));
    TEST_ASSERT_EQUAL_UINT32(1234u, aVar_asU32(&value));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
    TEST_ASSERT_EQUAL_HEX32(A_NULL, aVar_type(&value));
}

void testScalars_roundTripFloatingPoint(void)
{
    AVar value = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setDouble(&value, 3.25));
    TEST_ASSERT_EQUAL_HEX32(A_DOUBLE, aVar_type(&value));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 3.25, aVar_asDouble(&value));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setFloat32(&value, 1.5f));
    TEST_ASSERT_EQUAL_HEX32(A_FLOAT32, aVar_type(&value));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, aVar_asFloat32(&value));
}

void testScalars_copyPreservesValues(void)
{
    AVar source = {0};
    AVar copy = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&source, 777));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_copy(&source, &copy));

    TEST_ASSERT_EQUAL_HEX32(A_INT64, aVar_type(&copy));
    TEST_ASSERT_EQUAL_INT64(777, aVar_asI64(&copy));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&source));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&copy));
}
