#include "test-suite.h"

#include <string.h>

void testBuffers_borrowedStringDoesNotOwnMemory(void)
{
    AVar value = {0};
    size_t length = 0u;
    char text[] = "hello";

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setString(&value, text, false));
    TEST_ASSERT_EQUAL_HEX32(A_STRING, aVar_type(&value));
    TEST_ASSERT_BITS_LOW(A_FLAG_OWNED, value.type);
    TEST_ASSERT_EQUAL_STRING_LEN("hello", aVar_asString(&value, &length), length);
    TEST_ASSERT_EQUAL_UINT64(5u, (uint64_t)length);
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
}

void testBuffers_copiedStringIsIndependent(void)
{
    AVar value = {0};
    char text[] = "hello";
    size_t length = 0u;

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setString(&value, text, true));
    TEST_ASSERT_BITS_HIGH(A_FLAG_OWNED, value.type);

    text[0] = 'j';
    TEST_ASSERT_EQUAL_STRING_LEN("hello", aVar_asString(&value, &length), length);
    TEST_ASSERT_EQUAL_UINT64(5u, (uint64_t)length);

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
}

void testBuffers_copiedBinaryIsIndependent(void)
{
    AVar value = {0};
    uint8_t bytes[] = {1u, 2u, 3u, 4u};
    const uint8_t *stored;
    size_t length = 0u;

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setBinary(&value, bytes, sizeof(bytes), true));
    stored = (const uint8_t *)aVar_asBinary(&value, &length);

    TEST_ASSERT_EQUAL_UINT64(sizeof(bytes), (uint64_t)length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, stored, sizeof(bytes));

    bytes[0] = 99u;
    TEST_ASSERT_EQUAL_UINT8(1u, stored[0]);
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
}

void testBuffers_copyCreatesOwnedStorage(void)
{
    AVar source = {0};
    AVar copy = {0};
    size_t sourceLength = 0u;
    size_t copyLength = 0u;

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setString(&source, "world", true));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_copy(&source, &copy));
    TEST_ASSERT_BITS_HIGH(A_FLAG_OWNED, copy.type);
    TEST_ASSERT_EQUAL_STRING_LEN(aVar_asString(&source, &sourceLength),
                                 aVar_asString(&copy, &copyLength), copyLength);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)sourceLength, (uint64_t)copyLength);

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&source));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&copy));
}
