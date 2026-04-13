/**
 * @file anyvar.h
 * @brief Public C ABI for the AnyVar tagged-union runtime.
 * @author AnyVar contributors
 * @date 2026-04-12
 * @license MIT OR Apache-2.0
 *
 * Naming convention:
 * - Functions and variables use lowerCamelCase after the aVar_ prefix.
 * - Types, structs, and enums use PascalCase.
 * - Macros and constants use UPPER_SNAKE_CASE.
 * - Public symbols never use leading underscores or double underscores.
 *
 * Compile-time flags:
 * - A_NO_HEAP: disable all heap allocation paths and require caller-provided buffers.
 * - A_CUSTOM_ALLOC: enable custom allocator registration via aVar_setAllocator().
 * - A_NO_THREAD_SAFE: disable the internal global mutex.
 * - A_PACKED: request a packed AVar layout where supported by the compiler.
 * - A_NO_MAP: disable A_MAP support to reduce code size on constrained targets.
 */

#ifndef ANYVAR_H
#define ANYVAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint32_t AVarType;

enum {
    A_NULL = 0x00u,
    A_BOOL = 0x01u,
    A_INT64 = 0x20u,
    A_UINT64 = 0x21u,
    A_INT32 = 0x22u,
    A_UINT32 = 0x23u,
    A_DOUBLE = 0x28u,
    A_FLOAT32 = 0x29u,
    A_STRING = 0x40u,
    A_BINARY = 0x41u,
    A_ARRAY = 0x80u,
    A_MAP = 0x81u
};

#define A_FLAG_BOOL_VAL 0x00000100u
#define A_FLAG_OWNED 0x00000200u
#define A_FLAG_CONST 0x00000400u
#define A_FLAG_BACKEND 0x80000000u

#define A_BASE_TYPE(t) ((AVarType)((uint32_t)(t) & 0xFFu))
#define A_IS_BACKEND(t) ((((uint32_t)(t)) & A_FLAG_BACKEND) != 0u)
#define A_IS_NUMERIC(t) ((((uint32_t)A_BASE_TYPE(t)) & 0xF0u) == 0x20u)
#define A_IS_INTEGER(t) ((((uint32_t)A_BASE_TYPE(t)) & 0xF8u) == 0x20u)
#define A_IS_FLOAT(t) ((((uint32_t)A_BASE_TYPE(t)) & 0xF8u) == 0x28u)
#define A_IS_BUFFER(t) ((((uint32_t)A_BASE_TYPE(t)) & 0xF0u) == 0x40u)
#define A_IS_CONTAINER(t) ((((uint32_t)A_BASE_TYPE(t)) & 0xF0u) == 0x80u)
#define A_IS_UNSIGNED(t) (A_IS_INTEGER(t) && ((((uint32_t)A_BASE_TYPE(t)) & 0x01u) != 0u))
#define A_IS_32BIT_INT(t) (A_IS_INTEGER(t) && ((((uint32_t)A_BASE_TYPE(t)) & 0x02u) != 0u))

#define AVAR_OK 0
#define AVAR_ERR_TYPE (-1)
#define AVAR_ERR_NULL (-2)
#define AVAR_ERR_OOM (-3)
#define AVAR_ERR_BOUNDS (-4)
#define AVAR_ERR_KEY (-5)
#define AVAR_ERR_BACKEND (-6)
#define AVAR_ERR_ARGS (-7)

#if defined(A_PACKED) && (defined(__clang__) || defined(__GNUC__))
#define AVAR_PACKED __attribute__((packed))
#else
#define AVAR_PACKED
#endif

typedef struct AVar AVar;
typedef struct ABackend ABackend;

typedef void *(*AVarAllocFn)(size_t size, void *context);
typedef void *(*AVarReallocFn)(void *pointer, size_t size, void *context);
typedef void (*AVarFreeFn)(void *pointer, void *context);

/**
 * @brief Allocator hooks used when A_CUSTOM_ALLOC is enabled.
 */
typedef struct AVarAllocator {
    AVarAllocFn alloc;
    AVarReallocFn reallocFn;
    AVarFreeFn freeFn;
    void *context;
} AVarAllocator;

/**
 * @brief Borrowed or owned string/binary storage.
 */
typedef struct AVarString {
    char *data;
    size_t len;
} AVarString;

/**
 * @brief Array container view.
 */
typedef struct AVarArray {
    AVar *items;
    size_t len;
} AVarArray;

/**
 * @brief Map container view.
 *
 * Keys are stored as AVar string values so ownership and cleanup follow
 * the same rules as every other AVar instance.
 */
typedef struct AVarMap {
    AVar *keys;
    AVar *values;
    size_t len;
} AVarMap;

/**
 * @brief Backend-dispatched payload.
 */
typedef struct AVarBackendValue {
    const ABackend *vtable;
    void *data;
} AVarBackendValue;

/**
 * @brief Single standard-layout value used for native and backend paths.
 */
struct AVAR_PACKED AVar {
    AVarType type;
    union {
        int64_t i64;
        uint64_t u64;
        int32_t i32;
        uint32_t u32;
        double d;
        float f32;
        AVarString str;
        AVarArray array;
        AVarMap map;
        AVarBackendValue backend;
    } u;
};

typedef AVar AVarNative;

/**
 * @brief Backend vtable for pluggable runtime formats.
 */
struct ABackend {
    const char *name;
    AVarType (*getType)(const AVar *value);
    bool (*asBool)(const AVar *value);
    int64_t (*asI64)(const AVar *value);
    uint64_t (*asU64)(const AVar *value);
    int32_t (*asI32)(const AVar *value);
    uint32_t (*asU32)(const AVar *value);
    double (*asDouble)(const AVar *value);
    float (*asFloat32)(const AVar *value);
    const char *(*asString)(const AVar *value, size_t *outLength);
    const void *(*asBinary)(const AVar *value, size_t *outLength);
    void (*setNull)(AVar *value);
    void (*setBool)(AVar *value, bool flag);
    void (*setI64)(AVar *value, int64_t number);
    void (*setU64)(AVar *value, uint64_t number);
    void (*setI32)(AVar *value, int32_t number);
    void (*setU32)(AVar *value, uint32_t number);
    void (*setDouble)(AVar *value, double number);
    void (*setFloat32)(AVar *value, float number);
    void (*setString)(AVar *value, const char *string, size_t length, bool copy);
    void (*setBinary)(AVar *value, const void *bytes, size_t length, bool copy);
    size_t (*arrayLen)(const AVar *value);
    int (*arrayGet)(const AVar *value, size_t index, AVar *outItem);
    size_t (*mapLen)(const AVar *value);
    int (*mapGet)(const AVar *value, const char *key, AVar *outItem);
    int (*clear)(AVar *value);
    int (*copy)(const AVar *source, AVar *destination);
    int (*encodeCbor)(const AVar *value, uint8_t *buffer, size_t *bufferLength);
    int (*decodeCbor)(AVar *value, const uint8_t *buffer, size_t bufferLength);
    int (*encodeJson)(const AVar *value, char *buffer, size_t *bufferLength);
    int (*decodeJson)(AVar *value, const char *buffer);
};

extern const ABackend AVAR_CBOR_BACKEND;
extern const ABackend AVAR_JSON_BACKEND;

int aVar_setAllocator(const AVarAllocator *allocator);
void aVar_resetAllocator(void);

int aVar_clearImpl(AVar *value);
int aVar_copyImpl(const AVar *source, AVar *destination);
int aVar_setStringLen(AVar *value, const char *string, size_t length, bool copy);
int aVar_setBinary(AVar *value, const void *bytes, size_t length, bool copy);
int aVar_convert(const AVar *source, const ABackend *destinationBackend, AVar *destination);

int aVar_arrayInit(AVar *value, size_t initialCapacity);
int aVar_arrayInitFixed(AVar *value, void *buffer, size_t bufferSize);
size_t aVar_arrayLen(const AVar *value);
int aVar_arrayPush(AVar *value, const AVar *item);
int aVar_arrayGet(const AVar *value, size_t index, AVar *outItem);

int aVar_mapInit(AVar *value, size_t initialCapacity);
int aVar_mapInitFixed(AVar *value, void *buffer, size_t bufferSize);
size_t aVar_mapLen(const AVar *value);
int aVar_mapSet(AVar *value, const char *key, const AVar *entry, bool copyKey);
int aVar_mapGet(const AVar *value, const char *key, AVar *outItem);

/**
 * @brief Clear a value and release any owned storage.
 */
static inline int aVar_clear(AVar *value)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (!A_IS_BACKEND(value->type)) {
        return aVar_clearImpl(value);
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->clear == NULL) {
        return AVAR_ERR_BACKEND;
    }

    return value->u.backend.vtable->clear(value);
}

/**
 * @brief Deep-copy a value.
 */
static inline int aVar_copy(const AVar *source, AVar *destination)
{
    if (source == NULL || destination == NULL) {
        return AVAR_ERR_NULL;
    }

    if (!A_IS_BACKEND(source->type)) {
        return aVar_copyImpl(source, destination);
    }

    if (source->u.backend.vtable == NULL || source->u.backend.vtable->copy == NULL) {
        return AVAR_ERR_BACKEND;
    }

    return source->u.backend.vtable->copy(source, destination);
}

/**
 * @brief Return the current base type.
 */
static inline AVarType aVar_type(const AVar *value)
{
    if (value == NULL) {
        return A_NULL;
    }

    if (!A_IS_BACKEND(value->type)) {
        return A_BASE_TYPE(value->type);
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->getType == NULL) {
        return A_NULL;
    }

    return value->u.backend.vtable->getType(value);
}

static inline int aVar_setNull(AVar *value)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->setNull != NULL) {
        value->u.backend.vtable->setNull(value);
        return AVAR_OK;
    }

    if (aVar_clear(value) != AVAR_OK) {
        return AVAR_ERR_BACKEND;
    }

    value->type = A_NULL;
    return AVAR_OK;
}

static inline int aVar_setBool(AVar *value, bool flag)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->setBool != NULL) {
        value->u.backend.vtable->setBool(value, flag);
        return AVAR_OK;
    }

    if (aVar_clear(value) != AVAR_OK) {
        return AVAR_ERR_BACKEND;
    }

    value->type = A_BOOL | (flag ? A_FLAG_BOOL_VAL : 0u);
    return AVAR_OK;
}

static inline int aVar_setI64(AVar *value, int64_t number)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->setI64 != NULL) {
        value->u.backend.vtable->setI64(value, number);
        return AVAR_OK;
    }

    if (aVar_clear(value) != AVAR_OK) {
        return AVAR_ERR_BACKEND;
    }

    value->type = A_INT64;
    value->u.i64 = number;
    return AVAR_OK;
}

static inline int aVar_setU64(AVar *value, uint64_t number)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->setU64 != NULL) {
        value->u.backend.vtable->setU64(value, number);
        return AVAR_OK;
    }

    if (aVar_clear(value) != AVAR_OK) {
        return AVAR_ERR_BACKEND;
    }

    value->type = A_UINT64;
    value->u.u64 = number;
    return AVAR_OK;
}

static inline int aVar_setI32(AVar *value, int32_t number)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->setI32 != NULL) {
        value->u.backend.vtable->setI32(value, number);
        return AVAR_OK;
    }

    if (aVar_clear(value) != AVAR_OK) {
        return AVAR_ERR_BACKEND;
    }

    value->type = A_INT32;
    value->u.i32 = number;
    return AVAR_OK;
}

static inline int aVar_setU32(AVar *value, uint32_t number)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->setU32 != NULL) {
        value->u.backend.vtable->setU32(value, number);
        return AVAR_OK;
    }

    if (aVar_clear(value) != AVAR_OK) {
        return AVAR_ERR_BACKEND;
    }

    value->type = A_UINT32;
    value->u.u32 = number;
    return AVAR_OK;
}

static inline int aVar_setDouble(AVar *value, double number)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->setDouble != NULL) {
        value->u.backend.vtable->setDouble(value, number);
        return AVAR_OK;
    }

    if (aVar_clear(value) != AVAR_OK) {
        return AVAR_ERR_BACKEND;
    }

    value->type = A_DOUBLE;
    value->u.d = number;
    return AVAR_OK;
}

static inline int aVar_setFloat32(AVar *value, float number)
{
    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->setFloat32 != NULL) {
        value->u.backend.vtable->setFloat32(value, number);
        return AVAR_OK;
    }

    if (aVar_clear(value) != AVAR_OK) {
        return AVAR_ERR_BACKEND;
    }

    value->type = A_FLOAT32;
    value->u.f32 = number;
    return AVAR_OK;
}

static inline int aVar_setString(AVar *value, const char *string, bool copy)
{
    if (string == NULL) {
        return AVAR_ERR_NULL;
    }

    return aVar_setStringLen(value, string, strlen(string), copy);
}

static inline bool aVar_asBool(const AVar *value)
{
    if (value == NULL) {
        return false;
    }

    if (!A_IS_BACKEND(value->type)) {
        if (A_BASE_TYPE(value->type) != A_BOOL) {
            return false;
        }
        return (value->type & A_FLAG_BOOL_VAL) != 0u;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asBool == NULL) {
        return false;
    }

    return value->u.backend.vtable->asBool(value);
}

static inline int64_t aVar_asI64(const AVar *value)
{
    if (value == NULL) {
        return 0;
    }

    if (!A_IS_BACKEND(value->type)) {
        return A_BASE_TYPE(value->type) == A_INT64 ? value->u.i64 : 0;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asI64 == NULL) {
        return 0;
    }

    return value->u.backend.vtable->asI64(value);
}

static inline uint64_t aVar_asU64(const AVar *value)
{
    if (value == NULL) {
        return 0u;
    }

    if (!A_IS_BACKEND(value->type)) {
        return A_BASE_TYPE(value->type) == A_UINT64 ? value->u.u64 : 0u;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asU64 == NULL) {
        return 0u;
    }

    return value->u.backend.vtable->asU64(value);
}

static inline int32_t aVar_asI32(const AVar *value)
{
    if (value == NULL) {
        return 0;
    }

    if (!A_IS_BACKEND(value->type)) {
        return A_BASE_TYPE(value->type) == A_INT32 ? value->u.i32 : 0;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asI32 == NULL) {
        return 0;
    }

    return value->u.backend.vtable->asI32(value);
}

static inline uint32_t aVar_asU32(const AVar *value)
{
    if (value == NULL) {
        return 0u;
    }

    if (!A_IS_BACKEND(value->type)) {
        return A_BASE_TYPE(value->type) == A_UINT32 ? value->u.u32 : 0u;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asU32 == NULL) {
        return 0u;
    }

    return value->u.backend.vtable->asU32(value);
}

static inline double aVar_asDouble(const AVar *value)
{
    if (value == NULL) {
        return 0.0;
    }

    if (!A_IS_BACKEND(value->type)) {
        return A_BASE_TYPE(value->type) == A_DOUBLE ? value->u.d : 0.0;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asDouble == NULL) {
        return 0.0;
    }

    return value->u.backend.vtable->asDouble(value);
}

static inline float aVar_asFloat32(const AVar *value)
{
    if (value == NULL) {
        return 0.0f;
    }

    if (!A_IS_BACKEND(value->type)) {
        return A_BASE_TYPE(value->type) == A_FLOAT32 ? value->u.f32 : 0.0f;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asFloat32 == NULL) {
        return 0.0f;
    }

    return value->u.backend.vtable->asFloat32(value);
}

static inline const char *aVar_asString(const AVar *value, size_t *outLength)
{
    if (outLength != NULL) {
        *outLength = 0u;
    }

    if (value == NULL) {
        return NULL;
    }

    if (!A_IS_BACKEND(value->type)) {
        if (A_BASE_TYPE(value->type) != A_STRING) {
            return NULL;
        }
        if (outLength != NULL) {
            *outLength = value->u.str.len;
        }
        return value->u.str.data;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asString == NULL) {
        return NULL;
    }

    return value->u.backend.vtable->asString(value, outLength);
}

static inline const void *aVar_asBinary(const AVar *value, size_t *outLength)
{
    if (outLength != NULL) {
        *outLength = 0u;
    }

    if (value == NULL) {
        return NULL;
    }

    if (!A_IS_BACKEND(value->type)) {
        if (A_BASE_TYPE(value->type) != A_BINARY) {
            return NULL;
        }
        if (outLength != NULL) {
            *outLength = value->u.str.len;
        }
        return value->u.str.data;
    }

    if (value->u.backend.vtable == NULL || value->u.backend.vtable->asBinary == NULL) {
        return NULL;
    }

    return value->u.backend.vtable->asBinary(value, outLength);
}

#if UINTPTR_MAX > UINT32_MAX && !defined(A_PACKED)
_Static_assert(sizeof(AVar) == 32u, "AVar must be 32 bytes on 64-bit hosts");
#endif

_Static_assert(offsetof(AVar, type) == 0u, "AVar.type must be the first field");
_Static_assert(sizeof(AVarType) == sizeof(uint32_t), "AVarType must be 32 bits wide");

#ifdef __cplusplus
}
#endif

#endif
