#include "anyvar-internal.h"

#ifndef _WIN32
#include <pthread.h>
#endif

static void *avarDefaultAlloc(size_t size, void *context)
{
    (void)context;
    return malloc(size);
}

static void *avarDefaultRealloc(void *pointer, size_t size, void *context)
{
    (void)context;
    return realloc(pointer, size);
}

static void avarDefaultFree(void *pointer, void *context)
{
    (void)context;
    free(pointer);
}

AVarAllocator gAvarAllocator = {
    .alloc = avarDefaultAlloc,
    .reallocFn = avarDefaultRealloc,
    .freeFn = avarDefaultFree,
    .context = NULL,
};

#ifndef A_NO_THREAD_SAFE
#if defined(_WIN32)
#include <windows.h>
static INIT_ONCE gAvarOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION gAvarMutex;

static BOOL CALLBACK avarInitMutex(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&gAvarMutex);
    return TRUE;
}

void avarLock(void)
{
    InitOnceExecuteOnce(&gAvarOnce, avarInitMutex, NULL, NULL);
    EnterCriticalSection(&gAvarMutex);
}

void avarUnlock(void) { LeaveCriticalSection(&gAvarMutex); }
#else
static pthread_once_t gAvarOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t gAvarMutex;

static void avarInitMutex(void)
{
    pthread_mutexattr_t attributes;
    (void)pthread_mutexattr_init(&attributes);
    (void)pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    (void)pthread_mutex_init(&gAvarMutex, &attributes);
    (void)pthread_mutexattr_destroy(&attributes);
}

void avarLock(void)
{
    (void)pthread_once(&gAvarOnce, avarInitMutex);
    (void)pthread_mutex_lock(&gAvarMutex);
}

void avarUnlock(void) { (void)pthread_mutex_unlock(&gAvarMutex); }
#endif
#else
void avarLock(void) {}

void avarUnlock(void) {}
#endif

void *avarAlloc(size_t size)
{
#ifdef A_NO_HEAP
    (void)size;
    return NULL;
#else
    return gAvarAllocator.alloc != NULL ? gAvarAllocator.alloc(size, gAvarAllocator.context) : NULL;
#endif
}

void *avarRealloc(void *pointer, size_t size)
{
#ifdef A_NO_HEAP
    (void)pointer;
    (void)size;
    return NULL;
#else
    return gAvarAllocator.reallocFn != NULL
               ? gAvarAllocator.reallocFn(pointer, size, gAvarAllocator.context)
               : NULL;
#endif
}

void avarFree(void *pointer)
{
#ifndef A_NO_HEAP
    if (gAvarAllocator.freeFn != NULL) {
        gAvarAllocator.freeFn(pointer, gAvarAllocator.context);
    }
#else
    (void)pointer;
#endif
}

AVarContainerHeader *avarHeaderFromItems(const AVar *items)
{
    if (items == NULL) {
        return NULL; /* LCOV_EXCL_LINE */
    }

    return ((AVarContainerHeader *)items) - 1;
}

void avarResetValue(AVar *value)
{
    if (value == NULL) {
        return; /* LCOV_EXCL_LINE */
    }

    value->type = A_NULL;
    value->u.u64 = 0u;
}

static int avarAllocateBytes(char **destination, size_t length)
{
    if (destination == NULL) {
        return AVAR_ERR_NULL; /* LCOV_EXCL_LINE */
    }

    *destination = NULL;

    if (length == 0u) {
        *destination = (char *)avarAlloc(1u); /* LCOV_EXCL_LINE */
        if (*destination == NULL) {           /* LCOV_EXCL_LINE */
            return AVAR_ERR_OOM;              /* LCOV_EXCL_LINE */
        }
        (*destination)[0] = '\0'; /* LCOV_EXCL_LINE */
        return AVAR_OK;           /* LCOV_EXCL_LINE */
    }

    *destination = (char *)avarAlloc(length);
    return *destination != NULL ? AVAR_OK : AVAR_ERR_OOM;
}

static void avarCopyMemory(char *destination, const char *source, /* LCOV_EXCL_LINE */
                           size_t length)                         /* LCOV_EXCL_LINE */
{
    for (size_t index = 0u; index < length; ++index) { /* LCOV_EXCL_LINE */
        destination[index] = source[index];            /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static AVarBridgeBox *avarBridgeBox(const AVar *value) /* LCOV_EXCL_LINE */
{
    return value != NULL ? (AVarBridgeBox *)value->u.backend.data : NULL; /* LCOV_EXCL_LINE */
}

int avarBridgeExtract(const AVar *source, AVar *destination)
{
    AVarBridgeBox *box;

    if (source == NULL || destination == NULL) {
        return AVAR_ERR_NULL; /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */

    if (!A_IS_BACKEND(source->type)) {
        return avarCopyUnlocked(source, destination);
    }

    box = (AVarBridgeBox *)source->u.backend.data;
    if (box == NULL) {
        return AVAR_ERR_BACKEND;
    }

    return avarCopyUnlocked(&box->nativeValue, destination);
}

static AVarType avarBridgeGetType(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_type(&box->nativeValue) : A_NULL;
}

static bool avarBridgeAsBool(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asBool(&box->nativeValue) : false;
}

static int64_t avarBridgeAsI64(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asI64(&box->nativeValue) : 0;
}

static uint64_t avarBridgeAsU64(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asU64(&box->nativeValue) : 0u;
}

static int32_t avarBridgeAsI32(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asI32(&box->nativeValue) : 0;
}

static uint32_t avarBridgeAsU32(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asU32(&box->nativeValue) : 0u;
}

static double avarBridgeAsDouble(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asDouble(&box->nativeValue) : 0.0;
}

static float avarBridgeAsFloat32(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asFloat32(&box->nativeValue) : 0.0f;
}

static const char *avarBridgeAsString(const AVar *value, size_t *outLength)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asString(&box->nativeValue, outLength) : NULL;
}

static const void *avarBridgeAsBinary(const AVar *value, size_t *outLength)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_asBinary(&box->nativeValue, outLength) : NULL;
}

static int avarBridgeSetNative(AVar *value, const AVar *source) /* LCOV_EXCL_LINE */
{
    AVarBridgeBox *box;
    int status;

    if (value == NULL || source == NULL) { /* LCOV_EXCL_LINE */
        return AVAR_ERR_NULL;              /* LCOV_EXCL_LINE */
    }

    box = avarBridgeBox(value); /* LCOV_EXCL_LINE */
    if (box == NULL) {          /* LCOV_EXCL_LINE */
#ifdef A_NO_HEAP
        return AVAR_ERR_OOM;
#else
        box = (AVarBridgeBox *)avarAlloc(sizeof(*box)); /* LCOV_EXCL_LINE */
        if (box == NULL) {                              /* LCOV_EXCL_LINE */
            return AVAR_ERR_OOM;                        /* LCOV_EXCL_LINE */
        }
        avarResetValue(&box->nativeValue); /* LCOV_EXCL_LINE */
        value->type = A_FLAG_BACKEND;      /* LCOV_EXCL_LINE */
        value->u.backend.data = box;       /* LCOV_EXCL_LINE */
#endif
    } /* LCOV_EXCL_LINE */

    status = avarCopyUnlocked(source, &box->nativeValue); /* LCOV_EXCL_LINE */
    return status;                                        /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static void avarBridgeSetNull(AVar *value)
{
    AVar nullValue = {0};
    (void)avarBridgeSetNative(value, &nullValue);
}

static void avarBridgeSetBool(AVar *value, bool flag)
{
    AVar tmp = {0};
    (void)aVar_setBool(&tmp, flag);
    (void)avarBridgeSetNative(value, &tmp);
}

static void avarBridgeSetI64(AVar *value, int64_t number)
{
    AVar tmp = {0};
    (void)aVar_setI64(&tmp, number);
    (void)avarBridgeSetNative(value, &tmp);
}

static void avarBridgeSetU64(AVar *value, uint64_t number)
{
    AVar tmp = {0};
    (void)aVar_setU64(&tmp, number);
    (void)avarBridgeSetNative(value, &tmp);
}

static void avarBridgeSetI32(AVar *value, int32_t number)
{
    AVar tmp = {0};
    (void)aVar_setI32(&tmp, number);
    (void)avarBridgeSetNative(value, &tmp);
}

static void avarBridgeSetU32(AVar *value, uint32_t number)
{
    AVar tmp = {0};
    (void)aVar_setU32(&tmp, number);
    (void)avarBridgeSetNative(value, &tmp);
}

static void avarBridgeSetDouble(AVar *value, double number)
{
    AVar tmp = {0};
    (void)aVar_setDouble(&tmp, number);
    (void)avarBridgeSetNative(value, &tmp);
}

static void avarBridgeSetFloat32(AVar *value, float number)
{
    AVar tmp = {0};
    (void)aVar_setFloat32(&tmp, number);
    (void)avarBridgeSetNative(value, &tmp);
}

static void avarBridgeSetString(AVar *value, const char *string, size_t length, bool copy)
{
    AVar tmp = {0};
    if (aVar_setStringLen(&tmp, string, length, copy) == AVAR_OK) {
        (void)avarBridgeSetNative(value, &tmp);
        (void)aVar_clear(&tmp);
    }
}

static void avarBridgeSetBinary(AVar *value, const void *bytes, size_t length, bool copy)
{
    AVar tmp = {0};
    if (aVar_setBinary(&tmp, bytes, length, copy) == AVAR_OK) {
        (void)avarBridgeSetNative(value, &tmp);
        (void)aVar_clear(&tmp);
    }
}

static size_t avarBridgeArrayLen(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_arrayLen(&box->nativeValue) : 0u;
}

static int avarBridgeArrayGet(const AVar *value, size_t index, AVar *outItem)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_arrayGet(&box->nativeValue, index, outItem) : AVAR_ERR_BACKEND;
}

static size_t avarBridgeMapLen(const AVar *value)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_mapLen(&box->nativeValue) : 0u;
}

static int avarBridgeMapGet(const AVar *value, const char *key, AVar *outItem)
{
    AVarBridgeBox *box = avarBridgeBox(value);
    return box != NULL ? aVar_mapGet(&box->nativeValue, key, outItem) : AVAR_ERR_BACKEND;
}

static int avarBridgeClear(AVar *value)
{
    AVarBridgeBox *box;

    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    box = avarBridgeBox(value);
    if (box != NULL) {
        (void)avarClearUnlocked(&box->nativeValue);
        avarFree(box);
    }

    avarResetValue(value);
    return AVAR_OK;
}

static int avarBridgeCopy(const AVar *source, AVar *destination)
{
    AVarBridgeBox *box = avarBridgeBox(source);
    return box != NULL ? aVar_convert(&box->nativeValue, source->u.backend.vtable, destination)
                       : AVAR_ERR_BACKEND;
}

const ABackend AVAR_CBOR_BACKEND = {
    .name = "cbor",
    .getType = avarBridgeGetType,
    .asBool = avarBridgeAsBool,
    .asI64 = avarBridgeAsI64,
    .asU64 = avarBridgeAsU64,
    .asI32 = avarBridgeAsI32,
    .asU32 = avarBridgeAsU32,
    .asDouble = avarBridgeAsDouble,
    .asFloat32 = avarBridgeAsFloat32,
    .asString = avarBridgeAsString,
    .asBinary = avarBridgeAsBinary,
    .setNull = avarBridgeSetNull,
    .setBool = avarBridgeSetBool,
    .setI64 = avarBridgeSetI64,
    .setU64 = avarBridgeSetU64,
    .setI32 = avarBridgeSetI32,
    .setU32 = avarBridgeSetU32,
    .setDouble = avarBridgeSetDouble,
    .setFloat32 = avarBridgeSetFloat32,
    .setString = avarBridgeSetString,
    .setBinary = avarBridgeSetBinary,
    .arrayLen = avarBridgeArrayLen,
    .arrayGet = avarBridgeArrayGet,
    .mapLen = avarBridgeMapLen,
    .mapGet = avarBridgeMapGet,
    .clear = avarBridgeClear,
    .copy = avarBridgeCopy,
    .encodeCbor = NULL,
    .decodeCbor = NULL,
    .encodeJson = NULL,
    .decodeJson = NULL,
};

const ABackend AVAR_JSON_BACKEND = {
    .name = "json",
    .getType = avarBridgeGetType,
    .asBool = avarBridgeAsBool,
    .asI64 = avarBridgeAsI64,
    .asU64 = avarBridgeAsU64,
    .asI32 = avarBridgeAsI32,
    .asU32 = avarBridgeAsU32,
    .asDouble = avarBridgeAsDouble,
    .asFloat32 = avarBridgeAsFloat32,
    .asString = avarBridgeAsString,
    .asBinary = avarBridgeAsBinary,
    .setNull = avarBridgeSetNull,
    .setBool = avarBridgeSetBool,
    .setI64 = avarBridgeSetI64,
    .setU64 = avarBridgeSetU64,
    .setI32 = avarBridgeSetI32,
    .setU32 = avarBridgeSetU32,
    .setDouble = avarBridgeSetDouble,
    .setFloat32 = avarBridgeSetFloat32,
    .setString = avarBridgeSetString,
    .setBinary = avarBridgeSetBinary,
    .arrayLen = avarBridgeArrayLen,
    .arrayGet = avarBridgeArrayGet,
    .mapLen = avarBridgeMapLen,
    .mapGet = avarBridgeMapGet,
    .clear = avarBridgeClear,
    .copy = avarBridgeCopy,
    .encodeCbor = NULL,
    .decodeCbor = NULL,
    .encodeJson = NULL,
    .decodeJson = NULL,
};

int avarClearUnlocked(AVar *value)
{
    AVarContainerHeader *header;

    if (value == NULL) {
        return AVAR_ERR_NULL; /* LCOV_EXCL_LINE */
    }

    switch (A_BASE_TYPE(value->type)) {
    case A_NULL:
    case A_BOOL:
    case A_INT64:
    case A_UINT64:
    case A_INT32:
    case A_UINT32:
    case A_DOUBLE:
    case A_FLOAT32:
        avarResetValue(value);
        return AVAR_OK;
    case A_STRING:
    case A_BINARY:
        if ((value->type & A_FLAG_OWNED) != 0u && value->u.str.data != NULL) {
            avarFree(value->u.str.data);
        }
        avarResetValue(value);
        return AVAR_OK;
    case A_ARRAY:
        header = avarHeaderFromItems(value->u.array.items);
        for (size_t index = 0u; index < value->u.array.len; ++index) {
            (void)avarClearUnlocked(&value->u.array.items[index]);
        }
        if (header != NULL && (header->flags & AVAR_CONTAINER_FIXED) == 0u) {
            avarFree(header);
        }
        avarResetValue(value);
        return AVAR_OK;
    case A_MAP:
#ifndef A_NO_MAP
        header = avarHeaderFromItems(value->u.map.keys);
        for (size_t index = 0u; index < value->u.map.len; ++index) {
            (void)avarClearUnlocked(&value->u.map.keys[index]);
            (void)avarClearUnlocked(&value->u.map.values[index]);
        }
        if (header != NULL && (header->flags & AVAR_CONTAINER_FIXED) == 0u) {
            avarFree(header);
        }
        avarResetValue(value);
        return AVAR_OK;
#else
        return AVAR_ERR_TYPE; /* LCOV_EXCL_LINE */
#endif
    default:
        return AVAR_ERR_TYPE; /* LCOV_EXCL_LINE */
    }
}

int avarCopyUnlocked(const AVar *source, AVar *destination)
{
    int status;
    const char *keyString;
    size_t keyLength;

    if (source == NULL || destination == NULL) {
        return AVAR_ERR_NULL; /* LCOV_EXCL_LINE */
    }

    if (destination != source) {
        (void)avarClearUnlocked(destination);
    }

    switch (A_BASE_TYPE(source->type)) {
    case A_NULL:
        avarResetValue(destination);
        return AVAR_OK;
    case A_BOOL:
        destination->type = A_BOOL | (source->type & A_FLAG_BOOL_VAL);
        return AVAR_OK;
    case A_INT64:
        destination->type = A_INT64;
        destination->u.i64 = source->u.i64;
        return AVAR_OK;
    case A_UINT64:
        destination->type = A_UINT64;
        destination->u.u64 = source->u.u64;
        return AVAR_OK;
    case A_INT32:
        destination->type = A_INT32;
        destination->u.i32 = source->u.i32;
        return AVAR_OK;
    case A_UINT32:
        destination->type = A_UINT32;
        destination->u.u32 = source->u.u32;
        return AVAR_OK;
    case A_DOUBLE:
        destination->type = A_DOUBLE;
        destination->u.d = source->u.d;
        return AVAR_OK;
    case A_FLOAT32:
        destination->type = A_FLOAT32;
        destination->u.f32 = source->u.f32;
        return AVAR_OK;
    case A_STRING:
        return aVar_setStringLen(destination, source->u.str.data, source->u.str.len, true);
    case A_BINARY:
        return aVar_setBinary(destination, source->u.str.data, source->u.str.len, true);
    case A_ARRAY:
        status = aVar_arrayInit(destination, source->u.array.len == 0u ? 1u : source->u.array.len);
        if (status != AVAR_OK) {
            return status; /* LCOV_EXCL_LINE */
        }
        for (size_t index = 0u; index < source->u.array.len; ++index) {
            status = aVar_arrayPush(destination, &source->u.array.items[index]);
            if (status != AVAR_OK) {
                (void)avarClearUnlocked(destination); /* LCOV_EXCL_LINE */
                return status;                        /* LCOV_EXCL_LINE */
            }
        }
        return AVAR_OK;
    case A_MAP:
#ifndef A_NO_MAP
        status = aVar_mapInit(destination, source->u.map.len == 0u ? 1u : source->u.map.len);
        if (status != AVAR_OK) {
            return status; /* LCOV_EXCL_LINE */
        }
        for (size_t index = 0u; index < source->u.map.len; ++index) {
            keyString = aVar_asString(&source->u.map.keys[index], &keyLength);
            if (keyString == NULL) {
                (void)avarClearUnlocked(destination);
                return AVAR_ERR_TYPE;
            }
            status = aVar_mapSet(destination, keyString, &source->u.map.values[index], true);
            if (status != AVAR_OK) {
                (void)avarClearUnlocked(destination); /* LCOV_EXCL_LINE */
                return status;                        /* LCOV_EXCL_LINE */
            }
        }
        return AVAR_OK;
#else
        return AVAR_ERR_TYPE; /* LCOV_EXCL_LINE */
#endif
    default:
        return AVAR_ERR_TYPE; /* LCOV_EXCL_LINE */
    }
}

int aVar_setAllocator(const AVarAllocator *allocator)
{
    if (allocator == NULL || allocator->alloc == NULL || allocator->reallocFn == NULL ||
        allocator->freeFn == NULL) {
        return AVAR_ERR_ARGS;
    }

    avarLock();
    gAvarAllocator = *allocator;
    avarUnlock();
    return AVAR_OK;
}

void aVar_resetAllocator(void)
{
    avarLock();
    gAvarAllocator.alloc = avarDefaultAlloc;
    gAvarAllocator.reallocFn = avarDefaultRealloc;
    gAvarAllocator.freeFn = avarDefaultFree;
    gAvarAllocator.context = NULL;
    avarUnlock();
}

int aVar_clearImpl(AVar *value)
{
    int status;
    avarLock();
    status = avarClearUnlocked(value);
    avarUnlock();
    return status;
}

int aVar_copyImpl(const AVar *source, AVar *destination)
{
    int status;
    avarLock();
    status = avarCopyUnlocked(source, destination);
    avarUnlock();
    return status;
}

int aVar_setStringLen(AVar *value, const char *string, size_t length, bool copy)
{
    int status;
    char *storage;

    if (value == NULL || string == NULL) {
        return AVAR_ERR_NULL; /* LCOV_EXCL_LINE */
    }

    avarLock();
    status = avarClearUnlocked(value);
    if (status != AVAR_OK) {
        avarUnlock();
        return status; /* LCOV_EXCL_LINE */
    }

    if (copy) {
        status = avarAllocateBytes(&storage, length + 1u);
        if (status != AVAR_OK) {
            avarUnlock();
            return status;
        }
        if (length > 0u) {
            avarCopyMemory(storage, string, length);
        }
        storage[length] = '\0';
        value->type = A_STRING | A_FLAG_OWNED;
        value->u.str.data = storage;
        value->u.str.len = length;
    } else {
        value->type = A_STRING;
        value->u.str.data = (char *)string;
        value->u.str.len = length;
    }

    avarUnlock();
    return AVAR_OK;
}

int aVar_setBinary(AVar *value, const void *bytes, size_t length, bool copy)
{
    int status;
    char *storage;

    if (value == NULL) {
        return AVAR_ERR_NULL; /* LCOV_EXCL_LINE */
    }

    avarLock();
    status = avarClearUnlocked(value);
    if (status != AVAR_OK) {
        avarUnlock();
        return status;
    }

    if (copy) {
        status = avarAllocateBytes(&storage, length == 0u ? 1u : length);
        if (status != AVAR_OK) {
            avarUnlock();
            return status;
        }
        if (length > 0u && bytes != NULL) {
            avarCopyMemory(storage, (const char *)bytes, length);
        }
        value->type = A_BINARY | A_FLAG_OWNED;
        value->u.str.data = storage;
        value->u.str.len = length;
    } else {
        value->type = A_BINARY;            /* LCOV_EXCL_LINE */
        value->u.str.data = (char *)bytes; /* LCOV_EXCL_LINE */
        value->u.str.len = length;         /* LCOV_EXCL_LINE */
    }

    avarUnlock();
    return AVAR_OK;
}

int aVar_convert(const AVar *source, const ABackend *destinationBackend, /* LCOV_EXCL_LINE */
                 AVar *destination)                                      /* LCOV_EXCL_LINE */
{
    AVarBridgeBox *box;
    int status;

    if (source == NULL || destination == NULL) { /* LCOV_EXCL_LINE */
        return AVAR_ERR_NULL;                    /* LCOV_EXCL_LINE */
    }

    avarLock();                                          /* LCOV_EXCL_LINE */
    if (destinationBackend == NULL) {                    /* LCOV_EXCL_LINE */
        status = avarBridgeExtract(source, destination); /* LCOV_EXCL_LINE */
        avarUnlock();                                    /* LCOV_EXCL_LINE */
        return status;                                   /* LCOV_EXCL_LINE */
    }

#ifdef A_NO_HEAP
    avarUnlock();
    return AVAR_ERR_OOM;
#else
    status = avarClearUnlocked(destination); /* LCOV_EXCL_LINE */
    if (status != AVAR_OK) {                 /* LCOV_EXCL_LINE */
        avarUnlock();                        /* LCOV_EXCL_LINE */
        return status;                       /* LCOV_EXCL_LINE */
    }

    box = (AVarBridgeBox *)avarAlloc(sizeof(*box)); /* LCOV_EXCL_LINE */
    if (box == NULL) {                              /* LCOV_EXCL_LINE */
        avarUnlock();                               /* LCOV_EXCL_LINE */
        return AVAR_ERR_OOM;                        /* LCOV_EXCL_LINE */
    }

    avarResetValue(&box->nativeValue);                     /* LCOV_EXCL_LINE */
    status = avarBridgeExtract(source, &box->nativeValue); /* LCOV_EXCL_LINE */
    if (status != AVAR_OK) {                               /* LCOV_EXCL_LINE */
        avarFree(box);                                     /* LCOV_EXCL_LINE */
        avarUnlock();                                      /* LCOV_EXCL_LINE */
        return status;                                     /* LCOV_EXCL_LINE */
    }

    destination->type = A_FLAG_BACKEND;                 /* LCOV_EXCL_LINE */
    destination->u.backend.vtable = destinationBackend; /* LCOV_EXCL_LINE */
    destination->u.backend.data = box;                  /* LCOV_EXCL_LINE */
    avarUnlock();                                       /* LCOV_EXCL_LINE */
    return AVAR_OK;                                     /* LCOV_EXCL_LINE */
#endif
} /* LCOV_EXCL_LINE */
