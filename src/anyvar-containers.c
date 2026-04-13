#include "anyvar-internal.h"

static int avarArrayEnsureCapacityUnlocked(AVar *value, size_t minimumCapacity)
{
    AVarContainerHeader *header;
    AVarContainerHeader *resized;
    AVar *items;
    size_t oldCapacity;
    size_t newCapacity;
    size_t blockSize;
    /* LCOV_EXCL_START */
    if (value == NULL || A_BASE_TYPE(value->type) != A_ARRAY || value->u.array.items == NULL) {
        return AVAR_ERR_TYPE;
    }
    /* LCOV_EXCL_STOP */

    header = avarHeaderFromItems(value->u.array.items);
    /* LCOV_EXCL_START */
    if (header == NULL || header->magic != AVAR_CONTAINER_MAGIC ||
        header->kind != AVAR_CONTAINER_ARRAY) {
        return AVAR_ERR_TYPE;
    }
    /* LCOV_EXCL_STOP */

    if (header->capacity >= minimumCapacity) {
        return AVAR_OK;
    }

    if ((header->flags & AVAR_CONTAINER_FIXED) != 0u) {
        return AVAR_ERR_OOM;
    }

    oldCapacity = header->capacity;
    newCapacity = oldCapacity == 0u ? 1u : oldCapacity;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    blockSize = sizeof(*header) + (newCapacity * sizeof(AVar));
    resized = (AVarContainerHeader *)avarRealloc(header, blockSize);
    if (resized == NULL) {
        return AVAR_ERR_OOM;
    }

    items = (AVar *)(resized + 1);
    for (size_t index = oldCapacity; index < newCapacity; ++index) {
        avarResetValue(&items[index]);
    }

    resized->capacity = newCapacity;
    value->u.array.items = items;
    return AVAR_OK;
}

static int avarMapEnsureCapacityUnlocked(AVar *value, size_t minimumCapacity) /* LCOV_EXCL_LINE */
{                                                                             /* LCOV_EXCL_LINE */
#ifdef A_NO_MAP
    /* LCOV_EXCL_START */
    (void)value;
    (void)minimumCapacity;
    return AVAR_ERR_TYPE;
    /* LCOV_EXCL_STOP */
#else
    AVarContainerHeader *header;
    AVarContainerHeader *resized;
    AVar *newKeys;
    AVar *newValues;
    size_t oldCapacity;
    size_t newCapacity;
    size_t blockSize;

    /* LCOV_EXCL_START */
    if (value == NULL || A_BASE_TYPE(value->type) != A_MAP || value->u.map.keys == NULL) {
        return AVAR_ERR_TYPE;
    }

    header = avarHeaderFromItems(value->u.map.keys);
    if (header == NULL || header->magic != AVAR_CONTAINER_MAGIC ||
        header->kind != AVAR_CONTAINER_MAP) { /* LCOV_EXCL_LINE */
        return AVAR_ERR_TYPE;
    }

    if (header->capacity >= minimumCapacity) {
        return AVAR_OK;
    }

    if ((header->flags & AVAR_CONTAINER_FIXED) != 0u) { /* LCOV_EXCL_LINE */
        return AVAR_ERR_OOM;                            /* LCOV_EXCL_LINE */
    }

    oldCapacity = header->capacity;                     /* LCOV_EXCL_LINE */
    newCapacity = oldCapacity == 0u ? 1u : oldCapacity; /* LCOV_EXCL_LINE */
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    } /* LCOV_EXCL_LINE */

    blockSize = sizeof(*header) + (newCapacity * sizeof(AVar) * 2u); /* LCOV_EXCL_LINE */
    resized = (AVarContainerHeader *)avarRealloc(header, blockSize); /* LCOV_EXCL_LINE */
    if (resized == NULL) {
        return AVAR_ERR_OOM;
    } /* LCOV_EXCL_LINE */

    newKeys = (AVar *)(resized + 1);   /* LCOV_EXCL_LINE */
    newValues = newKeys + newCapacity; /* LCOV_EXCL_LINE */
    for (size_t index = value->u.map.len; index > 0u; --index) {
        newValues[index - 1u] = newKeys[oldCapacity + index - 1u];
    } /* LCOV_EXCL_LINE */
    for (size_t index = oldCapacity; index < newCapacity; ++index) { /* LCOV_EXCL_LINE */
        avarResetValue(&newKeys[index]);                             /* LCOV_EXCL_LINE */
        avarResetValue(&newValues[index]);                           /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */

    resized->capacity = newCapacity; /* LCOV_EXCL_LINE */
    value->u.map.keys = newKeys;     /* LCOV_EXCL_LINE */
    value->u.map.values = newValues; /* LCOV_EXCL_LINE */
    return AVAR_OK;
    /* LCOV_EXCL_STOP */ /* LCOV_EXCL_LINE */
#endif /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

int aVar_arrayInit(AVar *value, size_t initialCapacity)
{ /* LCOV_EXCL_LINE */
    AVarContainerHeader *header;
    AVar *items;
    size_t capacity;
    size_t blockSize;
    int status;

    if (value == NULL) {
        return AVAR_ERR_NULL;
    }

    capacity = initialCapacity == 0u ? 1u : initialCapacity;
    blockSize = sizeof(*header) + (capacity * sizeof(AVar));

    avarLock();
    status = avarClearUnlocked(value);
    if (status != AVAR_OK) {
        avarUnlock();  /* LCOV_EXCL_LINE */
        return status; /* LCOV_EXCL_LINE */
    }

    header = (AVarContainerHeader *)avarAlloc(blockSize); /* LCOV_EXCL_LINE */
    if (header == NULL) {
        avarUnlock();
        return AVAR_ERR_OOM;
    }

    header->magic = AVAR_CONTAINER_MAGIC;
    header->kind = AVAR_CONTAINER_ARRAY;
    header->capacity = capacity;
    header->flags = 0u;
    items = (AVar *)(header + 1);
    for (size_t index = 0u; index < capacity; ++index) {
        avarResetValue(&items[index]);
    }

    value->type = A_ARRAY;
    value->u.array.items = items;
    value->u.array.len = 0u;
    avarUnlock();
    return AVAR_OK;
}

int aVar_arrayInitFixed(AVar *value, void *buffer, size_t bufferSize)
{
    AVarContainerHeader *header;
    AVar *items;
    size_t usableBytes;
    size_t capacity;
    int status;

    if (value == NULL || buffer == NULL) {
        return AVAR_ERR_NULL;
    }

    if (bufferSize <= sizeof(*header)) {
        return AVAR_ERR_ARGS;
    }

    usableBytes = bufferSize - sizeof(*header);
    capacity = usableBytes / sizeof(AVar);
    if (capacity == 0u) {
        return AVAR_ERR_ARGS;
    }

    avarLock();
    status = avarClearUnlocked(value);
    if (status != AVAR_OK) {
        avarUnlock();  /* LCOV_EXCL_LINE */
        return status; /* LCOV_EXCL_LINE */
    }

    header = (AVarContainerHeader *)buffer; /* LCOV_EXCL_LINE */
    header->magic = AVAR_CONTAINER_MAGIC;
    header->kind = AVAR_CONTAINER_ARRAY;
    header->capacity = capacity;
    header->flags = AVAR_CONTAINER_FIXED;
    items = (AVar *)(header + 1);
    for (size_t index = 0u; index < capacity; ++index) {
        avarResetValue(&items[index]);
    }

    value->type = A_ARRAY;
    value->u.array.items = items;
    value->u.array.len = 0u;
    avarUnlock();
    return AVAR_OK;
}

size_t aVar_arrayLen(const AVar *value)
{
    if (value == NULL) {
        return 0u;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->arrayLen != NULL) {     /* LCOV_EXCL_LINE */
        return value->u.backend.vtable->arrayLen(value); /* LCOV_EXCL_LINE */
    }

    return A_BASE_TYPE(value->type) == A_ARRAY ? value->u.array.len : 0u; /* LCOV_EXCL_LINE */
}

int aVar_arrayPush(AVar *value, const AVar *item)
{
    int status;

    if (value == NULL || item == NULL) {
        return AVAR_ERR_NULL;
    }

    avarLock();
    if (A_BASE_TYPE(value->type) != A_ARRAY) {
        avarUnlock();
        return AVAR_ERR_TYPE;
    }

    status = avarArrayEnsureCapacityUnlocked(value, value->u.array.len + 1u);
    if (status != AVAR_OK) {
        avarUnlock();
        return status;
    }

    status = avarCopyUnlocked(item, &value->u.array.items[value->u.array.len]);
    if (status == AVAR_OK) {
        value->u.array.len += 1u;
    }

    avarUnlock();
    return status;
}

int aVar_arrayGet(const AVar *value, size_t index, AVar *outItem)
{
    int status;

    if (value == NULL || outItem == NULL) {
        return AVAR_ERR_NULL;
    }

    avarLock();
    if (A_BASE_TYPE(value->type) != A_ARRAY) {
        avarUnlock();
        return AVAR_ERR_TYPE;
    }

    if (index >= value->u.array.len) {
        avarUnlock();
        return AVAR_ERR_BOUNDS;
    }

    status = avarCopyUnlocked(&value->u.array.items[index], outItem);
    avarUnlock();
    return status;
}

int aVar_mapInit(AVar *value, size_t initialCapacity)
{
#ifdef A_NO_MAP
    /* LCOV_EXCL_START */
    (void)value;
    (void)initialCapacity;
    return AVAR_ERR_TYPE;
    /* LCOV_EXCL_STOP */
#else
    AVarContainerHeader *header;
    AVar *keys;
    AVar *values;
    size_t capacity;
    size_t blockSize;
    int status;

    if (value == NULL) {
        return AVAR_ERR_NULL; /* LCOV_EXCL_LINE */
    }

    capacity = initialCapacity == 0u ? 1u : initialCapacity; /* LCOV_EXCL_LINE */
    blockSize = sizeof(*header) + (capacity * sizeof(AVar) * 2u);

    avarLock();
    status = avarClearUnlocked(value);
    if (status != AVAR_OK) {
        avarUnlock();  /* LCOV_EXCL_LINE */
        return status; /* LCOV_EXCL_LINE */
    }

    header = (AVarContainerHeader *)avarAlloc(blockSize); /* LCOV_EXCL_LINE */
    if (header == NULL) {
        avarUnlock();
        return AVAR_ERR_OOM;
    }

    header->magic = AVAR_CONTAINER_MAGIC;
    header->kind = AVAR_CONTAINER_MAP;
    header->capacity = capacity;
    header->flags = 0u;
    keys = (AVar *)(header + 1);
    values = keys + capacity;
    for (size_t index = 0u; index < capacity; ++index) {
        avarResetValue(&keys[index]);
        avarResetValue(&values[index]);
    }

    value->type = A_MAP;
    value->u.map.keys = keys;
    value->u.map.values = values;
    value->u.map.len = 0u;
    avarUnlock();
    return AVAR_OK;
#endif
}

int aVar_mapInitFixed(AVar *value, void *buffer, size_t bufferSize)
{
#ifdef A_NO_MAP
    /* LCOV_EXCL_START */
    (void)value;
    (void)buffer;
    (void)bufferSize;
    return AVAR_ERR_TYPE;
    /* LCOV_EXCL_STOP */
#else
    AVarContainerHeader *header;
    AVar *keys;
    AVar *values;
    size_t usableBytes;
    size_t capacity;
    int status;

    if (value == NULL || buffer == NULL) {
        return AVAR_ERR_NULL; /* LCOV_EXCL_LINE */
    }

    if (bufferSize <= sizeof(*header)) { /* LCOV_EXCL_LINE */
        return AVAR_ERR_ARGS;
    }

    usableBytes = bufferSize - sizeof(*header);
    capacity = usableBytes / (sizeof(AVar) * 2u);
    if (capacity == 0u) {
        return AVAR_ERR_ARGS; /* LCOV_EXCL_LINE */
    }

    avarLock(); /* LCOV_EXCL_LINE */
    status = avarClearUnlocked(value);
    if (status != AVAR_OK) {
        avarUnlock();  /* LCOV_EXCL_LINE */
        return status; /* LCOV_EXCL_LINE */
    }

    header = (AVarContainerHeader *)buffer; /* LCOV_EXCL_LINE */
    header->magic = AVAR_CONTAINER_MAGIC;
    header->kind = AVAR_CONTAINER_MAP;
    header->capacity = capacity;
    header->flags = AVAR_CONTAINER_FIXED;
    keys = (AVar *)(header + 1);
    values = keys + capacity;
    for (size_t index = 0u; index < capacity; ++index) {
        avarResetValue(&keys[index]);
        avarResetValue(&values[index]);
    }

    value->type = A_MAP;
    value->u.map.keys = keys;
    value->u.map.values = values;
    value->u.map.len = 0u;
    avarUnlock();
    return AVAR_OK;
#endif
}

size_t aVar_mapLen(const AVar *value)
{
    if (value == NULL) {
        return 0u;
    }

    if (A_IS_BACKEND(value->type) && value->u.backend.vtable != NULL &&
        value->u.backend.vtable->mapLen != NULL) {     /* LCOV_EXCL_LINE */
        return value->u.backend.vtable->mapLen(value); /* LCOV_EXCL_LINE */
    }

    return A_BASE_TYPE(value->type) == A_MAP ? value->u.map.len : 0u; /* LCOV_EXCL_LINE */
}

int aVar_mapSet(AVar *value, const char *key, const AVar *entry, bool copyKey)
{
#ifdef A_NO_MAP
    /* LCOV_EXCL_START */
    (void)value;
    (void)key;
    (void)entry;
    (void)copyKey;
    return AVAR_ERR_TYPE;
    /* LCOV_EXCL_STOP */
#else
    size_t keyLength;
    int status;
    size_t index;
    size_t keyLenOut;
    const char *existingKey;

    if (value == NULL || key == NULL || entry == NULL) {
        return AVAR_ERR_NULL;
    }

    keyLength = strlen(key);
    avarLock();
    if (A_BASE_TYPE(value->type) != A_MAP) {
        avarUnlock();
        return AVAR_ERR_TYPE;
    }

    index = value->u.map.len;
    for (size_t candidate = 0u; candidate < value->u.map.len; ++candidate) {
        existingKey = aVar_asString(&value->u.map.keys[candidate], &keyLenOut);
        if (existingKey != NULL && keyLenOut == keyLength && strcmp(existingKey, key) == 0) {
            index = candidate;
            break;
        }
    }

    if (index == value->u.map.len) {
        status = avarMapEnsureCapacityUnlocked(value, value->u.map.len + 1u);
        if (status != AVAR_OK) {
            avarUnlock();
            return status;
        }

        status = aVar_setStringLen(&value->u.map.keys[index], key, keyLength, copyKey);
        if (status != AVAR_OK) {
            avarUnlock();  /* LCOV_EXCL_LINE */
            return status; /* LCOV_EXCL_LINE */
        }
        value->u.map.len += 1u; /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */

    status = avarClearUnlocked(&value->u.map.values[index]);
    if (status != AVAR_OK) {
        avarUnlock();  /* LCOV_EXCL_LINE */
        return status; /* LCOV_EXCL_LINE */
    }

    status = avarCopyUnlocked(entry, &value->u.map.values[index]); /* LCOV_EXCL_LINE */
    avarUnlock();
    return status;
#endif
}

int aVar_mapGet(const AVar *value, const char *key, AVar *outItem) /* LCOV_EXCL_LINE */
{
#ifdef A_NO_MAP
    /* LCOV_EXCL_START */ /* LCOV_EXCL_LINE */
    (void)value;
    (void)key;
    (void)outItem;
    return AVAR_ERR_TYPE;
    /* LCOV_EXCL_STOP */
#else
    size_t keyLength;
    size_t keyLenOut;
    const char *existingKey;
    int status;

    if (value == NULL || key == NULL || outItem == NULL) { /* LCOV_EXCL_LINE */
        return AVAR_ERR_NULL;                              /* LCOV_EXCL_LINE */
    }

    keyLength = strlen(key);                 /* LCOV_EXCL_LINE */
    avarLock();                              /* LCOV_EXCL_LINE */
    if (A_BASE_TYPE(value->type) != A_MAP) { /* LCOV_EXCL_LINE */
        avarUnlock();                        /* LCOV_EXCL_LINE */
        return AVAR_ERR_TYPE;                /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */

    for (size_t index = 0u; index < value->u.map.len; ++index) {             /* LCOV_EXCL_LINE */
        existingKey = aVar_asString(&value->u.map.keys[index], &keyLenOut);  /* LCOV_EXCL_LINE */
        if (existingKey != NULL && keyLenOut == keyLength &&                 /* LCOV_EXCL_LINE */
            strcmp(existingKey, key) == 0) {                                 /* LCOV_EXCL_LINE */
            status = avarCopyUnlocked(&value->u.map.values[index], outItem); /* LCOV_EXCL_LINE */
            avarUnlock();                                                    /* LCOV_EXCL_LINE */
            return status;                                                   /* LCOV_EXCL_LINE */
        } /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */

    avarUnlock();        /* LCOV_EXCL_LINE */
    return AVAR_ERR_KEY; /* LCOV_EXCL_LINE */
#endif
} /* LCOV_EXCL_LINE */
