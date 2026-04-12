# AVar Implementation Thoughts

**Date:** April 2026
**Context:** Design discussion on backend architecture for AnyVar, informed by AnyFlux integration requirements.

---

## The Core Question

How should `AVar` handle the common case (native C struct, no serialization backend) without paying unnecessary overhead, while still supporting pluggable backends (CBOR, JSON, custom) when needed?

Four approaches were identified and compared.

---

## The Four Approaches

### 1. Always-Backend (current v0.2)

`AVar` is always a fat pointer: 8-byte `_backend` vtable pointer + 8-byte `_storage` union.

```c
typedef struct AVar {
    const ABackend* _backend;   /* NULL → AVAR_DEFAULT_BACKEND */
    union {
        void*    _ptr;
        int64_t  _i64;
        double   _d;
        uint8_t  _buf[8];
    } _storage;
} AVar;
```

**Size:** 16 bytes.

**Critical problem:** `_storage` is only 8 bytes. There is no room for both a type tag (`AVarType`) and a full 64-bit value simultaneously. This means the default backend must either:
- Point `_ptr` to a heap-allocated `AVarNative` — meaning **even scalars require a heap allocation**, or
- Encode the type in `_buf[0]` and value in the remaining 7 bytes — which is lossy for `int64` and `double`

In practice, the default path heap-allocates for all non-scalar types, and the "inline scalar" optimization is fragile.

---

### 2. Two-Types

`AVarNative` is the raw zero-overhead tagged union, used directly on hot paths. `AVar` is the fat pointer, used only at backend/FFI boundaries. Explicit box/unbox calls cross between them.

**Pros:**
- Truly zero overhead on native path — direct struct member access, no hidden fields
- `AVarNative` maps directly to FFI layouts (ctypes, Go unsafe, etc.)
- Compiler can fully inline and optimize native-path code

**Cons:**
- Two types in the API surface — components must handle both or always box/unbox at boundaries
- Box/unbox calls add friction and are easy to forget
- Generic code (serializers, loggers) must be written twice or templated
- Container types (`A_ARRAY`, `A_MAP`) store `AVarNative*` internally — if elements need to be backend-polymorphic, a different container type is needed too

**Best when:** Embedded/constrained targets are first-class and per-element memory matters more than API ergonomics.

---

### 3. Null-Backend Fast Path

`AVar` is a single unified type. `_storage` is sized to hold `AVarNative` inline. When `_backend == NULL`, helper functions operate directly on the native fields. When non-NULL, they dispatch through the vtable.

```c
typedef struct AVar {
    const ABackend* _backend;   /* NULL → native fast path            */
    union {
        AVarNative  _native;    /* inline when _backend == NULL       */
        void*       _ptr;       /* backend-allocated when non-NULL    */
    } _storage;
} AVar;
```

**Size:** ~40 bytes (8B backend ptr + ~32B AVarNative).

```c
static inline void a_var_set_i64(AVar* v, int64_t val) {
    if (__builtin_expect(v->_backend == NULL, 1)) {
        v->_storage._native.type  = A_INT64;
        v->_storage._native.u.i64 = val;
        return;
    }
    v->_backend->set_i64(v, val);
}
```

**Best when:** Performance on the native path is the priority and a single unified type is preferred. Superseded by approach 4 below, but a valid intermediate step.

---

### 4. Type-Sentinel (recommended)

Reserve one `AVarType` value (255) as a sentinel meaning "this instance dispatches via a backend". No extra field is added — the `type` field already present in `AVarNative` doubles as the discriminator. On the native path `AVar` is byte-for-byte identical to `AVarNative`.

```c
typedef enum AVarType {
    A_NULL    = 0,
    A_BOOL    = 1,
    A_INT64   = 2,
    A_DOUBLE  = 3,
    A_STRING  = 4,
    A_BINARY  = 5,
    A_ARRAY   = 6,
    A_MAP     = 7,
    /* 8–254 reserved for future native types */
    A_BACKEND = 255   /* sentinel: dispatch via u.backend vtable */
} AVarType;

typedef struct AVar AVar;
typedef struct ABackend ABackend;

struct AVar {
    AVarType type;      /* first field — always readable, no indirection   */
    union {
        /* native path: type 0–254 */
        bool    b;
        int64_t i64;
        double  d;
        struct { char* data; size_t len; bool owned; } str;
        struct { struct AVar* items; size_t len; } array;
        struct { struct AVar* keys; struct AVar* values; size_t len; } map;

        /* backend path: type == A_BACKEND */
        struct {
            const ABackend* vtable;
            void*           data;
        } backend;
    } u;
};
```

**Size:** ~32 bytes — identical to `AVarNative`. No extra field whatsoever on native instances.

Helper pattern:

```c
static inline AVarType a_var_type(const AVar* v) {
    if (__builtin_expect(v->type != A_BACKEND, 1))
        return v->type;                          /* direct read, first field */
    return v->u.backend.vtable->get_type(v);
}

static inline void a_var_set_i64(AVar* v, int64_t val) {
    if (__builtin_expect(v->type != A_BACKEND, 1)) {
        v->type  = A_INT64;
        v->u.i64 = val;
        return;
    }
    v->u.backend.vtable->set_i64(v, val);
}

static inline void a_var_clear(AVar* v) {
    if (__builtin_expect(v->type != A_BACKEND, 1)) {
        if ((v->type == A_STRING || v->type == A_BINARY) && v->u.str.owned)
            free(v->u.str.data);
        v->type = A_NULL;
        return;
    }
    v->u.backend.vtable->clear(v);
}
```

Usage is identical to null-backend from the caller's perspective:

```c
AVar v = {0};                                    /* type = A_NULL, native   */
a_var_set_i64(&v, 42);                           /* inline, no malloc       */
a_var_set_string(&v, "hello", 5, false);         /* borrowed, no malloc     */
a_var_clear(&v);

/* Opt into a backend explicitly */
AVar cbor = a_var_convert(&v, &AVAR_CBOR_BACKEND);  /* type = A_BACKEND    */
uint8_t buf[64]; size_t len = sizeof(buf);
cbor.u.backend.vtable->encode_cbor(&cbor, buf, &len);
a_var_clear(&cbor);
```

**Note on `A_CUSTOM`:** The current spec uses `255` for `A_CUSTOM` (opaque pointer extension). With this approach, custom extension types are simply backends with their own vtable — `A_BACKEND` supersedes `A_CUSTOM`. This is more principled: backends are already the extension mechanism, so collapsing both into `A_BACKEND` simplifies the type system rather than adding to it.

**Best when:** Always — this is strictly better than null-backend on every axis for the native path.

---

## Detailed Comparison: Always-Backend vs Null-Backend vs Type-Sentinel

### The Type Tag Problem

Always-backend has no type tag field in `AVar`. With only 8 bytes of `_storage`, storing both `AVarType` and a 64-bit value is impossible without tricks. In practice the default backend heap-allocates even for scalars.

Null-backend: `_native.type` is accessible as `v->_storage._native.type` — one level of nesting.

Type-sentinel: `v->type` is the very first field of `AVar` — readable with zero indirection, no branch, no nesting.

### Per-Value Cost

| Value type | Always-backend | Null-backend | Type-sentinel |
|---|---|---|---|
| Scalar (int64, double) | 16B + heap alloc ≈ 48B + malloc | 40B inline, no alloc | 32B inline, no alloc |
| Borrowed string | 16B + heap alloc for str struct | 40B inline, no alloc | 32B inline, no alloc |
| Owned string | 16B + heap for str + heap for char* | 40B inline + heap for char* only | 32B inline + heap for char* only |
| `a_var_type()` | vtable call or `_buf[0]` trick | `_storage._native.type` (nested) | `v->type` (first field, direct) |

### Array / Collection Access

**Always-backend** — array of 100 `AVar` scalars:
- 100 × 16B handles = 1,600B headers
- 100 heap allocations for values
- Traversal = 100 cache misses (each value elsewhere in heap)
- Total: ~4,800B + 100 `malloc` calls

**Null-backend** — same array:
- 100 × 40B = 4,000B, all contiguous, all values inline
- Traversal = sequential memory, hardware prefetcher works perfectly
- Total: 4,000B + 0 `malloc` calls

**Type-sentinel** — same array:
- 100 × 32B = 3,200B, all contiguous, all values inline
- Traversal = sequential memory, tightest packing of the three
- Total: 3,200B + 0 `malloc` calls

Type-sentinel uses the least memory of all three and has the best cache behaviour.

### Non-Default Backend Overhead

Always-backend: `_storage._ptr` holds backend data tightly — no wasted space.

Null-backend: the `_native` union member (~32B) sits unused when a backend is active — wastes ~32 bytes per instance on the non-default path.

Type-sentinel: `u.backend = { vtable*, data* }` = 16 bytes, which fits within the union (sized to `map` at ~24 bytes). The remaining ~8 bytes of the union are unused padding — waste is ~8 bytes, not ~32 bytes. Better than null-backend on the non-default path too.

All three: acceptable when the non-default path is not the hot path, as is the case in AnyFlux.

### Summary Table

| | Always-backend | Null-backend | Type-sentinel |
|---|---|---|---|
| Struct size (native) | 16B | ~40B | ~32B |
| Scalar — heap alloc? | Yes (type tag problem) | No | No |
| String — heap alloc? | Yes | No (borrowed), one alloc (owned) | No (borrowed), one alloc (owned) |
| Type tag access | Vtable call or trick | `_storage._native.type` (nested) | `v->type` (first field, direct) |
| `AVar` == `AVarNative` layout? | No | No | **Yes** |
| Collection cache behaviour | Poor (pointer-chasing) | Good (inline) | **Best** (tightest, inline) |
| Total memory for N scalars | ~48N + N mallocs | ~40N, 0 mallocs | **~32N, 0 mallocs** |
| Non-default backend waste | None | ~32B per instance | ~8B per instance |
| API surface | Single `AVar` type | Single `AVar` type | Single `AVar` type |

---

## Null-Backend: Full Example

> Included for reference. Type-sentinel (below) is the recommended approach.

```c
/* ── Type tag ─────────────────────────────────────────────────────────── */

typedef enum AVarType {
    A_NULL = 0, A_BOOL = 1, A_INT64 = 2, A_DOUBLE = 3,
    A_STRING = 4, A_BINARY = 5, A_ARRAY = 6, A_MAP = 7,
    A_CUSTOM = 255
} AVarType;

/* ── Native internal layout ───────────────────────────────────────────── */

typedef struct AVarNative {
    AVarType type;
    union {
        bool    b;
        int64_t i64;
        double  d;
        struct { char* data; size_t len; bool owned; } str;
        struct { struct AVarNative* items; size_t len; } array;
        struct { struct AVarNative* keys;
                 struct AVarNative* values; size_t len; } map;
        void*   custom;
    } u;
} AVarNative;

typedef struct AVar    AVar;
typedef struct ABackend ABackend;

/* ── Universal handle ─────────────────────────────────────────────────── */

struct AVar {
    const ABackend* _backend;   /* NULL → native fast path            */
    union {
        AVarNative  _native;    /* inline when _backend == NULL       */
        void*       _ptr;       /* backend-allocated when non-NULL    */
    } _storage;
};

/* ── Helper functions with null-backend fast path ─────────────────────── */

static inline AVarType a_var_type(const AVar* v) {
    if (__builtin_expect(v->_backend == NULL, 1))
        return v->_storage._native.type;
    return v->_backend->get_type(v);
}

static inline void a_var_set_i64(AVar* v, int64_t val) {
    if (__builtin_expect(v->_backend == NULL, 1)) {
        v->_storage._native.type  = A_INT64;
        v->_storage._native.u.i64 = val;
        return;
    }
    v->_backend->set_i64(v, val);
}

static inline void a_var_set_string(AVar* v, const char* s, size_t len, bool copy) {
    if (__builtin_expect(v->_backend == NULL, 1)) {
        AVarNative* n = &v->_storage._native;
        n->type = A_STRING;
        if (copy) {
            n->u.str.data  = (char*)malloc(len + 1);
            memcpy(n->u.str.data, s, len);
            n->u.str.data[len] = '\0';
            n->u.str.owned = true;
        } else {
            n->u.str.data  = (char*)s;
            n->u.str.owned = false;
        }
        n->u.str.len = len;
        return;
    }
    v->_backend->set_string(v, s, len, copy);
}

static inline void a_var_clear(AVar* v) {
    if (__builtin_expect(v->_backend == NULL, 1)) {
        AVarNative* n = &v->_storage._native;
        if ((n->type == A_STRING || n->type == A_BINARY) && n->u.str.owned)
            free(n->u.str.data);
        n->type = A_NULL;
        return;
    }
    v->_backend->clear(v);
    v->_backend = NULL;
}
```

**Usage:**

```c
/* Default path — zero-init, no setup, no vtable, no heap for scalars */
AVar v = {0};

a_var_set_i64(&v, 42);                            /* inline, no malloc  */
a_var_set_string(&v, "hello", 5, false);          /* borrowed, no malloc */
a_var_set_string(&v, "hello", 5, true);           /* owned, one malloc  */
a_var_clear(&v);

/* Switch to CBOR backend when serialization is needed */
AVar cbor = a_var_convert(&v, &AVAR_CBOR_BACKEND);
uint8_t buf[64]; size_t len = sizeof(buf);
cbor._backend->encode_cbor(&cbor, buf, &len);
a_var_clear(&cbor);
```

---

## Recommendation

The **type-sentinel** approach is the right design for AnyVar:

1. **`AVar` is byte-for-byte `AVarNative` on the native path** — zero extra fields, zero overhead
2. **`v->type` is the first field** — always readable with a single load, no nesting, no branch
3. **Tightest collection layout** — ~32B per element, fully inline, best cache behaviour of all four approaches
4. **Single unified type** — no box/unbox API, generic code written once
5. **`__builtin_expect(v->type != A_BACKEND, 1)`** makes the backend branch a cold out-of-line path
6. **`A_BACKEND` supersedes `A_CUSTOM`** — custom extension types are just backends with their own vtable; cleaner type system
7. **Less non-default backend waste** than null-backend (~8B vs ~32B padding when backend is active)

The current v0.2 `always-backend` design should be replaced with type-sentinel. The `AVarNative` typedef can be kept as a documentation alias (`typedef AVar AVarNative`) for FFI binding documentation, but the struct itself only needs to be defined once.
