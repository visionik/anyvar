#include "test-suite.h"

#include <stddef.h>
#include <stdlib.h>

typedef struct CountingAllocator {
    size_t allocCalls;
    size_t reallocCalls;
    size_t freeCalls;
} CountingAllocator;

typedef struct FakeHeader {
    uint32_t magic;
    uint32_t kind;
    size_t capacity;
    uint32_t flags;
} FakeHeader;

static void *countingAlloc(size_t size, void *context)
{
    CountingAllocator *allocator = (CountingAllocator *)context;
    allocator->allocCalls += 1u;
    return malloc(size);
}

static void *countingRealloc(void *pointer, size_t size, void *context)
{
    CountingAllocator *allocator = (CountingAllocator *)context;
    allocator->reallocCalls += 1u;
    return realloc(pointer, size);
}

static void countingFree(void *pointer, void *context)
{
    CountingAllocator *allocator = (CountingAllocator *)context;
    allocator->freeCalls += 1u;
    free(pointer);
}

void testContainers_arrayPushGetAndLen(void)
{
    AVar array = {0};
    AVar item = {0};
    AVar out = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayInit(&array, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&item, 10));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayPush(&array, &item));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&item, 20));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayPush(&array, &item));
    TEST_ASSERT_EQUAL_UINT64(2u, (uint64_t)aVar_arrayLen(&array));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayGet(&array, 1u, &out));
    TEST_ASSERT_EQUAL_INT64(20, aVar_asI64(&out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&item));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&array));
}

void testContainers_fixedArrayRejectsOverflow(void)
{
    AVar array = {0};
    AVar item = {0};
    max_align_t arena[16];

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayInitFixed(&array, arena, sizeof(arena)));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&item, 1));

    while (aVar_arrayPush(&array, &item) == AVAR_OK) {
    }

    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, aVar_arrayPush(&array, &item));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&item));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&array));
}

void testContainers_mapSetGetOverwriteAndLen(void)
{
    AVar map = {0};
    AVar value = {0};
    AVar out = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInit(&map, 1u));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&value, 1));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&map, "a", &value, true));
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)aVar_mapLen(&map));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&value, 2));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&map, "a", &value, true));
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)aVar_mapLen(&map));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapGet(&map, "a", &out));
    TEST_ASSERT_EQUAL_INT64(2, aVar_asI64(&out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&map));
}

void testContainers_fixedMapBorrowedKeyWorks(void)
{
    AVar map = {0};
    AVar value = {0};
    AVar out = {0};
    max_align_t arena[16];

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInitFixed(&map, arena, sizeof(arena)));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&value, 11));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&map, "borrowed", &value, false));
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)aVar_mapLen(&map));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapGet(&map, "borrowed", &out));
    TEST_ASSERT_EQUAL_INT32(11, aVar_asI32(&out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&map));
}

void testContainers_mapGrowthKeepsEntries(void)
{
    AVar map = {0};
    AVar value = {0};
    AVar out = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInit(&map, 1u));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&value, 1));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&map, "a", &value, true));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&value, 2));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&map, "b", &value, true));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&value, 3));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&map, "c", &value, true));
    TEST_ASSERT_EQUAL_UINT64(3u, (uint64_t)aVar_mapLen(&map));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapGet(&map, "a", &out));
    TEST_ASSERT_EQUAL_INT32(1, aVar_asI32(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapGet(&map, "b", &out));
    TEST_ASSERT_EQUAL_INT32(2, aVar_asI32(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapGet(&map, "c", &out));
    TEST_ASSERT_EQUAL_INT32(3, aVar_asI32(&out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&map));
}

void testContainers_customAllocatorHooksAreUsed(void)
{
    CountingAllocator counters = {0};
    AVarAllocator allocator = {
        .alloc = countingAlloc,
        .reallocFn = countingRealloc,
        .freeFn = countingFree,
        .context = &counters,
    };
    AVar array = {0};
    AVar value = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setAllocator(&allocator));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayInit(&array, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&value, 7));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayPush(&array, &value));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayPush(&array, &value));
    TEST_ASSERT_TRUE((uint64_t)counters.allocCalls > 0u);
    TEST_ASSERT_TRUE((uint64_t)counters.reallocCalls > 0u);

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&array));
    TEST_ASSERT_TRUE((uint64_t)counters.freeCalls > 0u);
}

void testContainers_copyArrayAndMapValues(void)
{
    AVar array = {0};
    AVar arrayCopy = {0};
    AVar map = {0};
    AVar mapCopy = {0};
    AVar stringValue = {0};
    AVar numericValue = {0};
    AVar out = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayInit(&array, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setString(&stringValue, "copy-me", true));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayPush(&array, &stringValue));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_copy(&array, &arrayCopy));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&array));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayGet(&arrayCopy, 0u, &out));
    TEST_ASSERT_EQUAL_STRING("copy-me", aVar_asString(&out, NULL));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&arrayCopy));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&stringValue));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInit(&map, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setU32(&numericValue, 42u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&map, "answer", &numericValue, true));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_copy(&map, &mapCopy));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&map));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapGet(&mapCopy, "answer", &out));
    TEST_ASSERT_EQUAL_UINT32(42u, aVar_asU32(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&mapCopy));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&numericValue));
}

void testContainers_malformedContainersAndFixedMapOverflow(void)
{
    struct {
        FakeHeader header;
        AVar items[1];
    } bogusArrayStorage = {0};
    struct {
        FakeHeader header;
        AVar keys[1];
        AVar values[1];
    } bogusMapStorage = {0};
    AVar nullArray = {.type = A_ARRAY, .u.array = {.items = NULL, .len = 0u}};
    AVar bogusArray = {.type = A_ARRAY, .u.array = {.items = bogusArrayStorage.items, .len = 0u}};
    AVar nullMap = {.type = A_MAP, .u.map = {.keys = NULL, .values = NULL, .len = 0u}};
    AVar bogusMap = {
        .type = A_MAP,
        .u.map = {.keys = bogusMapStorage.keys, .values = bogusMapStorage.values, .len = 0u},
    };
    AVar value = {0};
    AVar defaultArray = {0};
    AVar fixedMap = {0};
    char tinyBuffer[1];
    max_align_t arena[128];
    int status = AVAR_OK;

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&value, 9));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_arrayPush(&nullArray, &value));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_arrayPush(&bogusArray, &value));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_mapSet(&nullMap, "x", &value, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_mapSet(&bogusMap, "x", &value, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_ARGS,
                          aVar_arrayInitFixed(&defaultArray, tinyBuffer, sizeof(tinyBuffer)));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_ARGS,
                          aVar_mapInitFixed(&defaultArray, tinyBuffer, sizeof(tinyBuffer)));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayInit(&defaultArray, 0u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayPush(&defaultArray, &value));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&defaultArray));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInitFixed(&fixedMap, arena, sizeof(arena)));
    for (size_t index = 0u; index < 256u; ++index) {
        char key[4];

        key[0] = 'k';
        key[1] = (char)('A' + (int)((index / 26u) % 26u));
        key[2] = (char)('a' + (int)(index % 26u));
        key[3] = '\0';
        status = aVar_mapSet(&fixedMap, key, &value, true);
        if (status != AVAR_OK) {
            break;
        }
    }
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, status);

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&fixedMap));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
}

void testContainers_publicNullAndTypeErrors(void)
{
    AVar value = {0};
    AVar item = {0};
    AVar out = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&item, 7));

    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)aVar_arrayLen(NULL));
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)aVar_mapLen(NULL));

    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_arrayPush(NULL, &item));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_arrayPush(&value, NULL));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_arrayPush(&value, &item));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_arrayGet(NULL, 0u, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_arrayGet(&value, 0u, NULL));

    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_mapSet(NULL, "k", &item, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_mapSet(&value, NULL, &item, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_mapSet(&value, "k", NULL, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_mapSet(&value, "k", &item, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_mapGet(NULL, "k", &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_mapGet(&value, NULL, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_mapGet(&value, "k", NULL));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&item));
}
