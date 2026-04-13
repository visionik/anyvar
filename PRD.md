# AnyVar — Product Requirements Document

**Version:** 0.1.0-draft
**Date:** 2026-04-12
**Status:** Awaiting approval

---

## Problem Statement

Cross-language systems — especially those involving C, TypeScript, Python, Go, and embedded runtimes — have no lightweight, ABI-stable, zero-dependency tagged union that can cross FFI boundaries without serialization overhead. Existing solutions are either too heavy (GObject, QVariant), too language-specific, or require external dependencies that make them unsuitable for embedded or WASM targets.

AnyVar (`AVar`) fills this gap: a single, skinny C struct with a stable ABI, a pluggable backend system, and idiomatic bindings for each consuming language.

---

## Goals

1. Provide a canonical, C-ABI-compatible tagged union usable across C, TypeScript (WASM + N-API), and future language bindings.
2. Run on desktop (Linux/macOS/Windows), embedded (FreeRTOS/Zephyr), and WASM targets with zero code-path changes — only compile-time flags differ.
3. Keep the hot (native) path inline and allocation-free for scalar and buffer types.
4. Establish a pluggable backend system so CBOR, JSON, and user-defined wire formats are first-class.
5. Maintain strict semver ABI compatibility: MINOR/PATCH releases never break existing binaries.

---

## Non-Goals

- Full GObject-style dynamic type system.
- Built-in transformation or collection functions.
- Language-specific features in the ABI layer (no C++ std::variant).
- Garbage collection or cycle detection.

---

## User Stories

- **As a C developer**, I want to pass heterogeneous values across function calls without defining custom tagged unions for each project.
- **As a TypeScript developer**, I want to receive AVar values from the C core in Node.js (via N-API) and in the browser (via WASM) using the same npm package.
- **As an embedded developer**, I want to use AnyVar on FreeRTOS with no heap allocations and a caller-provided backing buffer.
- **As a library integrator**, I want to implement a custom wire format (e.g. MessagePack) by providing an `ABackend` vtable without modifying AnyVar internals.
- **As a CI/CD operator**, I want automated test matrices across Linux, macOS, Windows, and WASM builds on every PR.

---

## Functional Requirements

### FR-1 — Core AVar struct
The `AVar` struct MUST be a single standard-layout struct usable on the native path (scalars, buffers, containers) and the backend path (vtable dispatch). The `type` field MUST be the first member. `AVarType` MUST be `uint32_t` (bits 0–7: base type; bits 8–30: per-value flags; bit 31: backend mode). Zero-initialisation (`AVar v = {0}`) MUST produce a valid `A_NULL`.

### FR-2 — Type system
The native path MUST support: `A_NULL`, `A_BOOL`, `A_INT64`, `A_UINT64`, `A_INT32`, `A_UINT32`, `A_DOUBLE`, `A_FLOAT32`, `A_STRING` (UTF-8), `A_BINARY`, `A_ARRAY`, `A_MAP`. Category masks (integer, float, buffer, container) MUST be defined as single bitwise operations. Type codes 0x02–0x1F and other unused ranges are reserved.

### FR-3 — Type flags
The following flags MUST be defined in bits 8–30 of the `type` field: `A_FLAG_BOOL_VAL` (bit 8), `A_FLAG_OWNED` (bit 9), `A_FLAG_CONST` (bit 10), `A_FLAG_BACKEND` (bit 31). No additional union slot MAY be used for flag storage.

### FR-4 — Helper API
The public C API MUST expose inline helpers for all scalar setters/readers, buffer setters/readers (with `copy` parameter), container readers, lifecycle (`aVar_clear`, `aVar_copy`), and cross-backend conversion (`aVar_convert`). All helpers MUST check `A_IS_BACKEND(v->type)` and dispatch accordingly; the native path MUST be inline.
Example names (per NFR-5): `aVar_setI64`, `aVar_asBool`, `aVar_setString`, `aVar_clear`, `aVar_copy`, `aVar_convert`.

### FR-5 — ABackend vtable
The `ABackend` struct MUST define function pointers for: type introspection (`getType`), all scalar/buffer readers (`asBool`, `asI64`, `asString`, etc.) and writers (`setBool`, `setI64`, `setString`, etc.), container readers (`arrayLen`, `arrayGet`, `mapLen`, `mapGet`), lifecycle (`clear`, `copy`), and optional serialisation (`encodeCbor`, `decodeCbor`, `encodeJson`, `decodeJson` — NULL if unsupported). Two built-in backends MUST be declared (`AVAR_CBOR_BACKEND`, `AVAR_JSON_BACKEND`) — implemented in Phase 2.

### FR-6 — Error handling
All API functions that can fail MUST return `int` (0 = success, negative = error code). A set of `AVAR_ERR_*` constants MUST be defined. No heap allocation MAY be used for error reporting. Debug builds SHOULD assert on invalid usage; release builds MUST return an error code.

### FR-7 — Container memory — three-tier strategy
- **Default** (no flags): `malloc`/`realloc` growth for `A_ARRAY` and `A_MAP`.
- **`A_NO_HEAP`**: fixed capacity — caller MUST provide a backing buffer and capacity at creation; no internal allocation.
- **`A_CUSTOM_ALLOC`**: caller registers `alloc`/`realloc`/`free` hooks at compile time or via a context pointer; RTOS pools and arena allocators MUST be supported.
All three tiers MUST pass the same test suite (parameterised by allocator).

### FR-8 — Map key semantics
`A_MAP` keys MUST be null-terminated C strings. Key lookup MUST use `strcmp`. Key storage follows the same ownership model as `A_STRING` (borrow vs owned via `A_FLAG_OWNED`).

### FR-9 — Ownership and lifetime
`A_FLAG_OWNED` (bit 9) indicates the buffer/container owns its data. `aVar_clear` MUST free owned data and recursively clear container children. `aVar_copy` MUST produce a deep copy with all owned flags set. Borrowed AVars (flag clear) MUST NOT free on clear.

### FR-10 — Thread safety
Thread safety MUST be ON by default. Compile with `-DA_NO_THREAD_SAFE` to disable all locking (zero overhead — recommended for single-threaded and embedded targets). When enabled, locking MUST protect individual helper calls; cross-call atomicity is the caller's responsibility.

### FR-11 — Compile-time configuration flags
The following flags MUST be supported: `A_NO_HEAP`, `A_CUSTOM_ALLOC`, `A_NO_THREAD_SAFE`, `A_PACKED` (struct packing for minimal size), `A_NO_MAP` (disable MAP type to save code size on tiny targets). All flags MUST be documented in the top-level header.

### FR-12 — CBOR backend (Phase 2)
A zero-dependency, hand-rolled CBOR encoder/decoder (RFC 8949) MUST be implemented and exposed as `AVAR_CBOR_BACKEND`. It MUST support all AVar native types, deterministic encoding mode, and operate on caller-provided byte buffers (no internal allocation).

### FR-13 — JSON backend (Phase 2)
A zero-dependency, hand-rolled JSON encoder/decoder MUST be implemented and exposed as `AVAR_JSON_BACKEND`. It MUST support all AVar native types and operate on caller-provided char buffers.

### FR-14 — TypeScript binding (Phase 3)
A single npm package MUST provide:
- **WASM path**: C core compiled via Emscripten; runs in browser and Node.js.
- **N-API path**: native addon for Node.js; loaded preferentially over WASM when available.
- Runtime detection MUST select N-API when available, falling back to WASM.
- The public TypeScript API MUST be idiomatic (`toVariant()` / `fromVariant()`) and fully typed.
- Tests MUST use Vitest and cover both WASM and N-API paths.

### FR-15 — CI matrix (GitHub Actions)
CI MUST run on: Linux (x86-64), macOS (arm64), Windows (x86-64), WASM (Emscripten). Each run MUST execute `task check` (lint + build + test). Coverage MUST be reported and enforced at 100% per module.

### FR-16 — Build system
Taskfile MUST be the sole user-facing entry point. CMake MUST drive C compilation underneath. Tasks MUST include: `build`, `build:wasm`, `build:embedded` (cross-compile example), `test`, `test:coverage`, `lint`, `fmt`, `check`, `clean`.

### FR-17 — C test framework
The C test suite MUST use [Unity](https://github.com/ThrowTheSwitch/Unity) (single-file, embedded-friendly, no heap required). TypeScript tests MUST use Vitest. Both test runners MUST be invoked via `task test`.

---

## Non-Functional Requirements

### NFR-1 — Language and dependencies
Core library: C11, zero external dependencies. No external headers (not even POSIX) required for the minimal scalar-only build.

### NFR-2 — ABI stability
Semver: MAJOR version bumps for any ABI-breaking change. MINOR/PATCH releases MUST NOT change struct layout, function signatures, or type tag values. `_Static_assert` checks MUST guard struct size and member offsets in the header.

### NFR-3 — Allocation minimisation
Scalar types (`A_NULL` through `A_FLOAT32`, `A_BOOL`) MUST require zero heap allocation on the native path. Buffer types (`A_STRING`, `A_BINARY`) with `copy=false` MUST require zero heap allocation. Embedded code paths with `A_NO_HEAP` MUST have zero heap calls reachable.

### NFR-4 — Native path performance
All native-path scalar get/set helpers MUST be `static inline`. The backend check (`A_IS_BACKEND`) MUST be a single bitwise AND with `__builtin_expect` for the non-backend case. No virtual dispatch or function pointer indirection on the native path.

### NFR-5 — Naming convention
All public symbols MUST follow this convention (applies to C ABI, TypeScript binding, and all future language bindings):
- Functions and variables: `lowerCamelCase` — e.g. `aVar_setI64`, `aVar_asBool`, `aVar_clear`
- Types, structs, enums: `PascalCase` — e.g. `AVar`, `ABackend`, `AVarType`
- Macros and constants: `UPPER_SNAKE` — e.g. `A_FLAG_OWNED`, `AVAR_MAX_BUFFER`, `AVAR_CBOR_BACKEND`
- Namespace prefix + `_` separator for all public symbols (e.g. `aVar_` for instance functions, `avar_` for module-level)
- ⊗ Leading `_` on any public ABI symbol (reserved by C standard)
- ⊗ Double underscores `__` anywhere
- ⊗ Mixed `camelCase_with_underscore` style
- The naming convention MUST be documented in a comment block at the top of the public header

### NFR-6 — Test coverage
100% line coverage overall and per-module, measured via `task test:coverage`. New source files MUST ship with corresponding test files in the same PR.

### NFR-7 — Platform support
Desktop: Linux (GCC/Clang), macOS (Clang), Windows (MSVC/Clang-cl). Embedded: FreeRTOS + GCC cross-compiler (ARM Cortex-M example). WASM: Emscripten 3.x.

### NFR-8 — Documentation
The top-level public header MUST include a naming convention comment block. Every public function MUST have a Doxygen-style doc comment. A `CHANGELOG.md` MUST be maintained using Keep a Changelog format.

---

## Success Metrics

- All CI matrix jobs green on first public release.
- AVar scalar round-trip (set → get → clear) benchmarks at < 5 ns on x86-64 (no backend dispatch).
- TypeScript binding round-trip (JS → WASM/N-API → JS) benchmarks at < 500 ns per value.
- Zero heap allocations in an `A_NO_HEAP` build confirmed by valgrind/ASan in CI.
- 100% test coverage on first merge to main.

---

**Generated by**: deft-setup skill (Phase 3 — Full path PRD)
