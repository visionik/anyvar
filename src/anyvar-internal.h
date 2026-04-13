#ifndef ANYVAR_INTERNAL_H
#define ANYVAR_INTERNAL_H

#include "anyvar.h"

#include <stdlib.h>

#define AVAR_CONTAINER_MAGIC 0x41565243u
#define AVAR_CONTAINER_ARRAY 1u
#define AVAR_CONTAINER_MAP 2u
#define AVAR_CONTAINER_FIXED 1u

typedef struct AVarContainerHeader {
    uint32_t magic;
    uint32_t kind;
    size_t capacity;
    uint32_t flags;
} AVarContainerHeader;

typedef struct AVarBridgeBox {
    AVar nativeValue;
} AVarBridgeBox;

extern AVarAllocator gAvarAllocator;

void avarLock(void);
void avarUnlock(void);

void *avarAlloc(size_t size);
void *avarRealloc(void *pointer, size_t size);
void avarFree(void *pointer);

AVarContainerHeader *avarHeaderFromItems(const AVar *items);
void avarResetValue(AVar *value);
int avarCopyUnlocked(const AVar *source, AVar *destination);
int avarClearUnlocked(AVar *value);
int avarBridgeExtract(const AVar *source, AVar *destination);

#endif
