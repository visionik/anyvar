# AVar Implementation Thoughts

**Date:** April 2026
**Context:** Design discussion on backend architecture for AnyVar, informed by AnyFlux integration requirements.

---

## The Core Question

How should `AVar` handle the common case (native C struct, no serialization backend) without paying unnecessary overhead, while still supporting pluggable backends (CBOR, JSON, custom) when needed?

Three approaches were identified and compared.

---

## The Three Approaches

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

### 3. Null-Backend Fast Path (recommended)

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

**Best when:** Performance on the native path is the priority and a single unified type is preferred. This is the right choice for AnyFlux.

---

## Detailed Comparison: Always-Backend vs Null-Backend

### The Type Tag Problem

Always-backend has no type tag field in `AVar`. With only 8 bytes of `_storage`, storing both `AVarType` and a 64-bit value is impossible without tricks. In practice the default backend heap-allocates even for scalars.

Null-backend: `_native.type` and `_native.u.i64` are both inline, full 64-bit precision, zero heap allocation.

### Per-Value Cost

| Value type | Always-backend | Null-backend |
|---|---|---|
| Scalar (int64, double) | 16B header + heap alloc (~32B) ≈ 48B + malloc | 40B inline, no alloc |
| Borrowed string | 16B header + heap alloc for str struct | 40B inline, no alloc |
| Owned string | 16B header + heap for str struct + heap for char* | 40B inline + heap for char* only |
| `a_var_type()` | vtable call or `_buf[0]` decode trick | `_native.type` direct read |

### Array / Collection Access

This is where null-backend wins most decisively.

**Always-backend** — array of 100 `AVar` scalars:
- 100 × 16B handles = 1,600B headers
- 100 heap allocations for values
- Traversal = 100 cache misses (each value elsewhere in heap)
- Total: ~4,800B + 100 `malloc` calls

**Null-backend** — same array:
- 100 × 40B = 4,000B, all contiguous, all values inline
- Traversal = sequential memory, hardware prefetcher works perfectly
- Total: 4,000B + 0 `malloc` calls

Despite the larger per-element size, null-backend uses **less total memory** for collections and has far better cache behaviour.

### Non-Default Backend Overhead

The one area always-backend wins: when using a non-default backend (CBOR, JSON), `_storage._ptr` holds the backend data with no wasted space. With null-backend, the `_native` union member (~32B) sits unused — wasting ~32 bytes per `AVar` on the non-default path.

This is acceptable when the non-default path is not the hot path, as is the case in AnyFlux packet flow.

### Summary Table

| | Always-backend | Null-backend |
|---|---|---|
| Struct size | 16B | ~40B |
| Scalar — heap alloc? | Yes (type tag problem) | No |
| String — heap alloc? | Yes | No (borrowed), one alloc (owned) |
| Type tag access | Vtable call or encoded trick | Direct field read |
| Collection cache behaviour | Poor (pointer-chasing) | Good (inline, contiguous) |
| Total memory for N scalars | ~48N + N mallocs | ~40N, zero mallocs |
| Non-default backend waste | None | ~32B per AVar unused |
| API surface | Single `AVar` type | Single `AVar` type |

---

## Null-Backend: Full Example

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

For AnyVar, the **null-backend fast path** approach is the right design given that:

1. The native/default path is the hot path — performance there matters most
2. A single unified `AVar` type is strongly preferable for API ergonomics and generic code
3. The 8-byte `_backend` overhead per element is acceptable at desktop scale
4. The ~32B waste on non-default backend instances is acceptable since those instances are not in the hot path
5. `__builtin_expect` makes the null-check effectively free under branch prediction

The current v0.2 `always-backend` design should be updated to embed `AVarNative` inline in `_storage` rather than using a small 8-byte union, resolving the type-tag problem and eliminating heap allocation on the default path.
