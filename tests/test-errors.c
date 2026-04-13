#include "test-suite.h"

#include <stdlib.h>

static uint32_t gSeed = 0xC0FFEEu;

typedef struct FailingAllocator {
    bool failAlloc;
    bool failRealloc;
} FailingAllocator;

static void *failingAlloc(size_t size, void *context)
{
    FailingAllocator *allocator = (FailingAllocator *)context;
    if (allocator->failAlloc) {
        return NULL;
    }
    return malloc(size);
}

static void *failingRealloc(void *pointer, size_t size, void *context)
{
    FailingAllocator *allocator = (FailingAllocator *)context;
    if (allocator->failRealloc) {
        return NULL;
    }
    return realloc(pointer, size);
}

static void failingFree(void *pointer, void *context)
{
    (void)context;
    free(pointer);
}

static uint32_t nextRandom(void)
{
    gSeed ^= gSeed << 13u;
    gSeed ^= gSeed >> 17u;
    gSeed ^= gSeed << 5u;
    return gSeed;
}

void testErrors_arrayGetOutOfBounds(void)
{
    AVar array = {0};
    AVar out = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayInit(&array, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_BOUNDS, aVar_arrayGet(&array, 0u, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&array));
}

void testErrors_mapMissingKey(void)
{
    AVar map = {0};
    AVar out = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInit(&map, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_KEY, aVar_mapGet(&map, "missing", &out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&map));
}

void testErrors_convertToBackendAndBack(void)
{
    AVar native = {0};
    AVar backend = {0};
    AVar copy = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&native, 555));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_convert(&native, &AVAR_CBOR_BACKEND, &backend));
    TEST_ASSERT_TRUE(A_IS_BACKEND(backend.type));
    TEST_ASSERT_EQUAL_INT64(555, aVar_asI64(&backend));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_convert(&backend, NULL, &copy));
    TEST_ASSERT_EQUAL_HEX32(A_INT64, aVar_type(&copy));
    TEST_ASSERT_EQUAL_INT64(555, aVar_asI64(&copy));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&copy));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&backend));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&native));
}

void testErrors_allocatorFailurePaths(void)
{
    FailingAllocator control = {.failAlloc = true, .failRealloc = false};
    AVarAllocator allocator = {
        .alloc = failingAlloc,
        .reallocFn = failingRealloc,
        .freeFn = failingFree,
        .context = &control,
    };
    AVar value = {0};
    AVar other = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setAllocator(&allocator));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, aVar_setStringLen(&value, "", 0u, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, aVar_setBinary(&value, NULL, 0u, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, aVar_arrayInit(&value, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, aVar_mapInit(&value, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, aVar_convert(&other, &AVAR_JSON_BACKEND, &value));

    control.failAlloc = false;
    control.failRealloc = true;
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayInit(&value, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&other, 1));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayPush(&value, &other));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, aVar_arrayPush(&value, &other));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInit(&value, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&value, "a", &other, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_OOM, aVar_mapSet(&value, "b", &other, true));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&other));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
}

void testErrors_backendScalarVtableCoversBridgePaths(void)
{
    AVar backend = {
        .type = A_FLAG_BACKEND,
        .u.backend = {.vtable = &AVAR_JSON_BACKEND, .data = NULL},
    };
    const uint8_t binary[] = {9u, 8u, 7u};
    size_t length = 0u;

    backend.u.backend.vtable->setBool(&backend, true);
    TEST_ASSERT_TRUE(aVar_asBool(&backend));

    backend.u.backend.vtable->setI64(&backend, -9);
    TEST_ASSERT_EQUAL_INT64(-9, aVar_asI64(&backend));

    backend.u.backend.vtable->setU64(&backend, 9u);
    TEST_ASSERT_EQUAL_UINT64(9u, aVar_asU64(&backend));

    backend.u.backend.vtable->setI32(&backend, -3);
    TEST_ASSERT_EQUAL_INT32(-3, aVar_asI32(&backend));

    backend.u.backend.vtable->setU32(&backend, 3u);
    TEST_ASSERT_EQUAL_UINT32(3u, aVar_asU32(&backend));

    backend.u.backend.vtable->setDouble(&backend, 2.75);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 2.75, aVar_asDouble(&backend));

    backend.u.backend.vtable->setFloat32(&backend, 4.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 4.5f, aVar_asFloat32(&backend));

    backend.u.backend.vtable->setString(&backend, "bridge", 6u, true);
    TEST_ASSERT_EQUAL_STRING_LEN("bridge", aVar_asString(&backend, &length), length);

    backend.u.backend.vtable->setBinary(&backend, binary, sizeof(binary), true);
    (void)aVar_asBinary(&backend, &length);
    TEST_ASSERT_EQUAL_UINT64(sizeof(binary), (uint64_t)length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(binary, aVar_asBinary(&backend, &length), sizeof(binary));

    backend.u.backend.vtable->setNull(&backend);
    TEST_ASSERT_EQUAL_HEX32(A_NULL, aVar_type(&backend));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&backend));
}

void testErrors_backendContainerVtableCoversBridgePaths(void)
{
    AVar array = {0};
    AVar map = {0};
    AVar value = {0};
    AVar backendArray = {0};
    AVar backendMap = {0};
    AVar out = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayInit(&array, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&value, 17));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_arrayPush(&array, &value));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_convert(&array, &AVAR_JSON_BACKEND, &backendArray));
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)backendArray.u.backend.vtable->arrayLen(&backendArray));
    TEST_ASSERT_EQUAL_INT(AVAR_OK,
                          backendArray.u.backend.vtable->arrayGet(&backendArray, 0u, &out));
    TEST_ASSERT_EQUAL_INT64(17, aVar_asI64(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInit(&map, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapSet(&map, "k", &value, true));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_convert(&map, &AVAR_CBOR_BACKEND, &backendMap));
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)backendMap.u.backend.vtable->mapLen(&backendMap));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, backendMap.u.backend.vtable->mapGet(&backendMap, "k", &out));
    TEST_ASSERT_EQUAL_INT64(17, aVar_asI64(&out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&backendMap));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&backendArray));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&map));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&array));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&value));
}

void testErrors_invalidArgumentsAndTypeMismatches(void)
{
    AVar value = {0};
    AVar out = {0};
    AVarAllocator badAllocator = {0};

    TEST_ASSERT_EQUAL_INT(AVAR_ERR_ARGS, aVar_setAllocator(NULL));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_ARGS, aVar_setAllocator(&badAllocator));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_arrayInit(NULL, 1u));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_arrayInitFixed(&value, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_ARGS, aVar_arrayInitFixed(&value, &value, sizeof(AVar)));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_arrayGet(&value, 0u, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_mapGet(&value, "x", &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_copy(NULL, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_copy(&value, NULL));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_convert(NULL, NULL, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, aVar_convert(&value, NULL, NULL));
}

void testErrors_malformedBackendWrappersReturnSafeDefaults(void)
{
    AVar backend = {
        .type = A_FLAG_BACKEND,
        .u.backend = {.vtable = &AVAR_JSON_BACKEND, .data = NULL},
    };
    AVar out = {0};
    size_t length = 99u;

    TEST_ASSERT_EQUAL_HEX32(A_NULL, AVAR_JSON_BACKEND.getType(&backend));
    TEST_ASSERT_FALSE(AVAR_JSON_BACKEND.asBool(&backend));
    TEST_ASSERT_EQUAL_INT64(0, AVAR_JSON_BACKEND.asI64(&backend));
    TEST_ASSERT_EQUAL_UINT64(0u, AVAR_JSON_BACKEND.asU64(&backend));
    TEST_ASSERT_EQUAL_INT32(0, AVAR_JSON_BACKEND.asI32(&backend));
    TEST_ASSERT_EQUAL_UINT32(0u, AVAR_JSON_BACKEND.asU32(&backend));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, AVAR_JSON_BACKEND.asDouble(&backend));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, AVAR_JSON_BACKEND.asFloat32(&backend));
    TEST_ASSERT_NULL(AVAR_JSON_BACKEND.asString(&backend, &length));
    TEST_ASSERT_NULL(AVAR_JSON_BACKEND.asBinary(&backend, &length));
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)AVAR_JSON_BACKEND.arrayLen(&backend));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_BACKEND, AVAR_JSON_BACKEND.arrayGet(&backend, 0u, &out));
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)AVAR_JSON_BACKEND.mapLen(&backend));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_BACKEND, AVAR_JSON_BACKEND.mapGet(&backend, "x", &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_BACKEND, aVar_copy(&backend, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_BACKEND, aVar_convert(&backend, NULL, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_BACKEND, aVar_convert(&backend, &AVAR_CBOR_BACKEND, &out));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&backend));
    TEST_ASSERT_EQUAL_HEX32(A_NULL, backend.type);
}

void testErrors_invalidInternalShapesReturnTypeErrors(void)
{
    AVar invalidType = {.type = 0x7Fu};
    AVar brokenMap = {0};
    AVar out = {0};
    max_align_t arena[16];

    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_clear(&invalidType));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_copy(&invalidType, &out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_mapInitFixed(&brokenMap, arena, sizeof(arena)));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI32(&brokenMap.u.map.keys[0], 5));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setBool(&brokenMap.u.map.values[0], true));
    brokenMap.u.map.len = 1u;

    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_copy(&brokenMap, &out));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&brokenMap));
}

void testErrors_zeroLengthBuffersAndBridgeEdgeCases(void)
{
    AVar emptyString = {0};
    AVar emptyBinary = {0};
    AVar native = {0};
    AVar copy = {0};
    AVar invalid = {.type = 0x7Fu};
    size_t length = 1u;

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setStringLen(&emptyString, "", 0u, true));
    TEST_ASSERT_EQUAL_STRING("", aVar_asString(&emptyString, &length));
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)length);

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setBinary(&emptyBinary, NULL, 0u, true));
    TEST_ASSERT_NOT_NULL(aVar_asBinary(&emptyBinary, &length));
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)length);

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setU32(&native, 77u));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_convert(&native, NULL, &copy));
    TEST_ASSERT_EQUAL_UINT32(77u, aVar_asU32(&copy));

    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_setStringLen(&invalid, "bad", 3u, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_setBinary(&invalid, "x", 1u, true));
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_TYPE, aVar_convert(&native, &AVAR_JSON_BACKEND, &invalid));

    AVAR_JSON_BACKEND.setNull(NULL);
    TEST_ASSERT_EQUAL_INT(AVAR_ERR_NULL, AVAR_JSON_BACKEND.clear(NULL));

    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&copy));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&native));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&emptyBinary));
    TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&emptyString));
}

void testErrors_randomizedRoundTrips(void)
{
    for (size_t index = 0u; index < 64u; ++index) {
        AVar source = {0};
        AVar copy = {0};
        int64_t value = (int64_t)(nextRandom() % 100000u);

        TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_setI64(&source, value));
        TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_copy(&source, &copy));
        TEST_ASSERT_EQUAL_INT64(value, aVar_asI64(&copy));
        TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&source));
        TEST_ASSERT_EQUAL_INT(AVAR_OK, aVar_clear(&copy));
    }
}
