# AnyVar — SPECIFICATION

**Version:** 0.1.0
**Date:** 2026-04-12
**Status:** Approved
**PRD:** [PRD.md](PRD.md)

---

## Overview

AnyVar (`AVar`) is a canonical, C-ABI-compatible tagged union for cross-language dynamic values.
It runs unchanged on desktop (Linux/macOS/Windows), embedded (FreeRTOS/Zephyr), and WASM
targets — only compile-time flags differ. The first binding is TypeScript (WASM + N-API).
Serialization backends (CBOR, JSON) are pluggable via an `ABackend` vtable.

## Architecture

Single `AVar` struct: `type` field (`uint32_t`, bits 0–7 base type, bits 8–30 flags, bit 31
backend mode) plus a union of scalar/buffer/container/backend slots. Native path is fully
inline. Backend path dispatches through `u.backend.vtable`. Three-tier container allocation:
dynamic (default), fixed-cap caller-provided buffer (`A_NO_HEAP`), custom hooks
(`A_CUSTOM_ALLOC`). Thread safety on by default; disabled via `A_NO_THREAD_SAFE`.
Build: Taskfile + CMake. Tests: Unity (C), Vitest (TypeScript). CI: GitHub Actions matrix.

## Requirements Reference

See [PRD.md](PRD.md) for full FR/NFR definitions. Task traces are listed as `(traces: FR-N / NFR-N)`.

---

## Phase Dependency Map

```
Phase 1: Core C Library          ← no dependencies
Phase 2: Serialization Backends  ← depends on Phase 1 complete
Phase 3: TypeScript Binding      ← depends on Phase 1 complete (Phase 2 for serialization tests only)
Phase 4: Embedded Hardening      ← depends on Phase 1 complete, independent of Phases 2 & 3
```

Phases 2, 3, and 4 may proceed in parallel once Phase 1 is complete.
Within Phase 2, subphases 2.1 (CBOR) and 2.2 (JSON) are independent.
Within Phase 3, subphases 3.1 (WASM) and 3.2 (N-API) are independent.

---

## Phase 1: Core C Library

### Subphase 1.1: Project Scaffolding

**1.1.1** CMakeLists.txt: library target, compile flags, install rules, toolchain file stubs for ARM/Emscripten
- Acceptance: `cmake --preset default` configures without error on Linux/macOS/Windows
- (traces: FR-16, NFR-7)

**1.1.2** Taskfile.yml: `build`, `build:wasm`, `build:embedded`, `test`, `test:coverage`, `lint`, `fmt`, `check`, `clean` tasks
- Acceptance: `task --list` shows all tasks; `task check` runs without error on clean checkout
- (traces: FR-16)

**1.1.3** GitHub Actions CI: matrix job (Linux x86-64, macOS arm64, Windows x86-64, WASM/Emscripten)
- Acceptance: All four matrix jobs run `task check` on every push to main and on PRs
- (traces: FR-15)

**1.1.4** Vendor Unity test framework (single-file, pin version)
- Acceptance: `Unity.h` and `unity.c` present under `vendor/unity/`; CMake test target links them
- (traces: FR-17)

**1.1.5** `CHANGELOG.md` skeleton (Keep a Changelog format) and `docs/` directory stub
- Acceptance: `CHANGELOG.md` has `[Unreleased]` section; `docs/` exists
- (traces: NFR-8)

---

### Subphase 1.2: AVar Struct and Type System (`anyvar.h`)

**1.2.1** Define `AVarType` enum (`uint32_t`), all type constants (`A_NULL` through `A_MAP`), reserved ranges
- Acceptance: All type tag values match spec; reserved ranges documented in header
- (traces: FR-1, FR-2)

**1.2.2** Define type flags (`A_FLAG_BOOL_VAL`, `A_FLAG_OWNED`, `A_FLAG_CONST`, `A_FLAG_BACKEND`) and category macros (`A_BASE_TYPE`, `A_IS_BACKEND`, `A_IS_NUMERIC`, `A_IS_INTEGER`, `A_IS_FLOAT`, `A_IS_BUFFER`, `A_IS_CONTAINER`)
- Acceptance: All macros are correct as single bitwise operations
- (traces: FR-3)

**1.2.3** Define `AVar` struct (`type` + union), `ABackend` forward declaration, `AVarNative` typedef alias
- Acceptance: `sizeof(AVar) == 32` on 64-bit desktop; `AVar v = {0}` is valid `A_NULL`; `_Static_assert` guards struct size and `type` field offset
- (traces: FR-1, NFR-2)

**1.2.4** Naming convention comment block at top of `anyvar.h`
- Acceptance: Header opens with comment documenting `lowerCamelCase`/`PascalCase`/`UPPER_SNAKE` rules and `aVar_` namespace prefix
- (traces: NFR-5, NFR-8)

**1.2.5** Compile-time configuration flags documented in header (`A_NO_HEAP`, `A_CUSTOM_ALLOC`, `A_NO_THREAD_SAFE`, `A_PACKED`, `A_NO_MAP`)
- Acceptance: All flags described with one-line comments; CMake exposes them as cache options
- (traces: FR-11)

---

### Subphase 1.3: Scalar and Buffer Helper API

**1.3.1** Inline scalar setters: `aVar_setNull`, `aVar_setBool`, `aVar_setI64`, `aVar_setU64`, `aVar_setI32`, `aVar_setU32`, `aVar_setDouble`, `aVar_setFloat32`
- Acceptance: Each is `static inline`; backend path dispatches via vtable; zero heap allocation
- (traces: FR-4, NFR-4)

**1.3.2** Inline scalar readers: `aVar_type`, `aVar_asBool`, `aVar_asI64`, `aVar_asU64`, `aVar_asI32`, `aVar_asU32`, `aVar_asDouble`, `aVar_asFloat32`
- Acceptance: Each is `static inline` with `__builtin_expect` hot-path hint
- (traces: FR-4, NFR-4)

**1.3.3** Buffer setters/readers: `aVar_setString`, `aVar_setBinary`, `aVar_asString`, `aVar_asBinary` (copy/borrow semantics via `bool copy` parameter)
- Acceptance: `copy=false` → borrowed AVar, zero malloc; `copy=true` → allocates and sets `A_FLAG_OWNED`
- (traces: FR-4, FR-9, NFR-3)

**1.3.4** Error return codes: `AVAR_OK`, `AVAR_ERR_TYPE`, `AVAR_ERR_NULL`, `AVAR_ERR_OOM`, `AVAR_ERR_BOUNDS`, `AVAR_ERR_KEY`
- Acceptance: All defined as negative `int` macros; zero heap used in error path
- (traces: FR-6)

---

### Subphase 1.4: ABackend Vtable

**1.4.1** Define `ABackend` struct with all function pointers: `getType`, scalar readers (`asBool`, `asI64`, `asU64`, `asI32`, `asU32`, `asDouble`, `asFloat32`, `asString`, `asBinary`), scalar writers (`setBool`, `setI64`, `setU64`, `setI32`, `setU32`, `setDouble`, `setFloat32`, `setString`, `setBinary`), container readers (`arrayLen`, `arrayGet`, `mapLen`, `mapGet`), lifecycle (`clear`, `copy`), optional serialisation (`encodeCbor`, `decodeCbor`, `encodeJson`, `decodeJson` — NULL if unsupported)
- Acceptance: `ABackend` compiles; `extern` declarations for `AVAR_CBOR_BACKEND` and `AVAR_JSON_BACKEND` present (undefined until Phase 2)
- (traces: FR-5)

**1.4.2** `aVar_convert`: cross-backend conversion helper
- Acceptance: `aVar_convert(&native, &AVAR_CBOR_BACKEND)` produces a backend AVar; basic round-trip test present
- (traces: FR-4)

---

### Subphase 1.5: Container Types (ARRAY and MAP)

**1.5.1** ARRAY — dynamic path: `aVar_arrayNew`, `aVar_arrayPush`, `aVar_arrayGet`, `aVar_arrayLen`, `aVar_arrayClear`
- Acceptance: Dynamic growth via `malloc`/`realloc`; code path disabled at compile time with `A_NO_HEAP`
- (traces: FR-7)

**1.5.2** MAP — dynamic path: `aVar_mapNew`, `aVar_mapSet`, `aVar_mapGet`, `aVar_mapLen`, `aVar_mapClear`; null-terminated string keys, `strcmp` lookup
- Acceptance: `strcmp` key lookup; key ownership follows `A_FLAG_OWNED`; duplicate key overwrites
- (traces: FR-7, FR-8)

**1.5.3** ARRAY + MAP — `A_NO_HEAP` path: caller-provided backing buffer, fixed capacity, no internal allocation
- Acceptance: With `-DA_NO_HEAP`, zero `malloc`/`realloc` calls reachable in the library; exceeding capacity returns `AVAR_ERR_OOM`
- (traces: FR-7, NFR-3)

**1.5.4** ARRAY + MAP — `A_CUSTOM_ALLOC` path: `alloc`/`realloc`/`free` hook registration
- Acceptance: Custom allocator hooks called in place of `malloc`/`realloc`/`free`; test with arena allocator stub
- (traces: FR-7)

---

### Subphase 1.6: Ownership, Lifecycle, and Thread Safety

**1.6.1** `aVar_clear`: free owned buffers/containers recursively; no-op for borrowed or scalar
- Acceptance: Valgrind reports zero leaks for owned `AVar` trees; borrowed AVars not freed
- (traces: FR-9, NFR-3)

**1.6.2** `aVar_copy`: deep copy, sets `A_FLAG_OWNED` on all heap-allocated nodes
- Acceptance: Modifying copy does not affect source
- (traces: FR-9)

**1.6.3** Thread safety: default-on mutex wrapping for all helper calls; `-DA_NO_THREAD_SAFE` disables all locking
- Acceptance: With `A_NO_THREAD_SAFE`, zero mutex symbols in linked binary; default build links platform mutex
- (traces: FR-10)

---

### Subphase 1.7: Phase 1 Test Suite (Unity)

**1.7.1** Scalar type tests: set/get/clear round-trips for all 8 scalar types; bool flag encoding; zero-init null
- Acceptance: 100% of scalar helpers covered; all tests green
- (traces: FR-2, FR-3, NFR-6)

**1.7.2** Buffer tests: owned/borrowed set, clear frees only owned, copy produces independent owned copy
- Acceptance: Zero leaks under Valgrind; copy independence verified
- (traces: FR-9, NFR-6)

**1.7.3** Container tests: array/map CRUD; exceed capacity (`A_NO_HEAP`); custom allocator path
- Acceptance: Same test file parameterised over all three allocator tiers
- (traces: FR-7, FR-8)

**1.7.4** Error code tests: type mismatch, null dereference, OOM, key-not-found
- Acceptance: All `AVAR_ERR_*` constants exercised
- (traces: FR-6)

**1.7.5** Coverage gate: `task test:coverage` enforces 100% line coverage per module
- Acceptance: CI fails if any module drops below 100%
- (traces: NFR-6)

---

## Phase 2: Serialization Backends

**Dependency:** Phase 1 complete.
Subphases 2.1 and 2.2 are independent and may be implemented in parallel.

### Subphase 2.1: CBOR Backend

**2.1.1** Hand-rolled CBOR encoder (RFC 8949): all `AVar` native types, deterministic mode, caller-provided output buffer
- Acceptance: Encoded output matches reference CBOR for all type samples; zero malloc
- (traces: FR-12)

**2.1.2** Hand-rolled CBOR decoder: caller-provided input buffer → `AVar`
- Acceptance: Round-trip encode/decode produces equal `AVar` for all type samples
- (traces: FR-12)

**2.1.3** `AVAR_CBOR_BACKEND`: implement `ABackend` vtable; expose `extern` constant
- Acceptance: `aVar_convert` to CBOR backend succeeds; `encodeCbor`/`decodeCbor` vtable slots populated
- (traces: FR-5, FR-12)

### Subphase 2.2: JSON Backend

**2.2.1** Hand-rolled JSON encoder: all `AVar` native types, caller-provided `char` buffer
- Acceptance: Output is valid JSON for all type samples; zero malloc
- (traces: FR-13)

**2.2.2** Hand-rolled JSON decoder: caller-provided JSON string → `AVar`
- Acceptance: Round-trip encode/decode produces equal `AVar`
- (traces: FR-13)

**2.2.3** `AVAR_JSON_BACKEND`: implement `ABackend` vtable; expose `extern` constant
- Acceptance: `aVar_convert` to JSON backend succeeds; `encodeJson`/`decodeJson` vtable slots populated
- (traces: FR-5, FR-13)

### Subphase 2.3: Serialization Tests

**2.3.1** Unity tests: CBOR and JSON round-trip for all native types including nested arrays/maps; error paths (buffer too small, malformed input)
- Acceptance: Full round-trip equality; all error paths covered; 100% coverage maintained
- (traces: FR-12, FR-13, NFR-6)

---

## Phase 3: TypeScript Binding

**Dependency:** Phase 1 complete. Phase 2 required only for serialization tests (3.4.1).
Subphases 3.1 (WASM) and 3.2 (N-API) are independent and may be implemented in parallel.

### Subphase 3.1: WASM Build (Emscripten)

**3.1.1** CMake Emscripten toolchain integration; `task build:wasm` produces `anyvar.wasm` + JS glue
- Acceptance: `task build:wasm` succeeds; `anyvar.wasm` loads in Node.js and browser
- (traces: FR-14, FR-16)

**3.1.2** Emscripten bindings: export all `aVar_*` helpers and `ABackend`
- Acceptance: All public API functions accessible from JS via `cwrap`/`ccall`
- (traces: FR-14)

### Subphase 3.2: N-API Native Addon

**3.2.1** N-API addon wrapping all `aVar_*` helpers; `cmake-js` build integration
- Acceptance: `require('./build/anyvar.node')` loads in Node.js 18+; all helpers callable
- (traces: FR-14)

### Subphase 3.3: npm Package and TypeScript API

**3.3.1** Runtime detection: load N-API addon when available, fall back to WASM
- Acceptance: Node.js + built addon → N-API selected; browser or missing addon → WASM selected
- (traces: FR-14)

**3.3.2** Idiomatic TypeScript API: `toVariant()`, `fromVariant()`, `AnyVar` class, full type definitions
- Acceptance: Compiles with `strict: true`; no `any` in public API; round-trip all native types
- (traces: FR-14, NFR-5)

**3.3.3** `package.json`: exports map, `types`, `engines`, bundled WASM asset, N-API addon as optional dep
- Acceptance: `npm pack` produces installable package with WASM included
- (traces: FR-14)

### Subphase 3.4: TypeScript Test Suite (Vitest)

**3.4.1** Vitest tests covering all native types via both WASM and N-API paths; 100% coverage
- Acceptance: Tests parameterised over both backends; CI runs both paths
- (traces: FR-14, FR-17, NFR-6)

---

## Phase 4: Embedded Hardening

**Dependency:** Phase 1 complete. Independent of Phases 2 and 3.

### Subphase 4.1: A_PACKED Layout and Size Verification

**4.1.1** `A_PACKED` build variant: struct packing attributes; `_Static_assert` packed size
- Acceptance: `sizeof(AVar) ≈ 24` bytes with `A_PACKED`; `_Static_assert` verifies
- (traces: FR-11, NFR-2)

### Subphase 4.2: ARM Cortex-M Cross-Compile Example

**4.2.1** CMake ARM Cortex-M toolchain file; `task build:embedded` produces ELF with `A_NO_HEAP + A_NO_THREAD_SAFE + A_PACKED`
- Acceptance: `arm-none-eabi-gcc` compiles cleanly; `nm` output shows zero `malloc`/`free` symbols
- (traces: FR-11, NFR-7)

### Subphase 4.3: Zero-Allocation CI Verification

**4.3.1** Valgrind/ASan CI job: run Phase 1 test suite with `A_NO_HEAP`; assert zero heap allocations
- Acceptance: Valgrind or ASan reports zero heap bytes allocated in `A_NO_HEAP` build
- (traces: NFR-3, FR-15)

---

## Testing Strategy

| Layer | Framework | Runner | Coverage Gate |
|---|---|---|---|
| C core | Unity (vendored) | `task test` | 100% per module |
| TypeScript binding | Vitest | `task test` | 100% per module |
| Zero-alloc verification | Valgrind / ASan | CI only | Zero heap in `A_NO_HEAP` build |

All new source files (`src/`, `include/`, `bindings/`) MUST ship with a corresponding test file in the same PR.

---

## Deployment

- **C library**: installed via CMake `install()` rules; consumed as static or shared lib
- **TypeScript**: published to npm as `anyvar` (single package, WASM + optional N-API)
- **Embedded**: consumed by including `anyvar.h` + compiling `anyvar.c` directly into the target firmware build

---

**Generated by**: deft-setup skill (Phase 3 — Full path, rendered from vbrief/specification.vbrief.json)
