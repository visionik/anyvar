# AnyVar

AnyVar is a small C11 tagged-union runtime for moving dynamic values across C ABI boundaries.
It is designed for desktop, embedded, and WASM targets with a single public header and a minimal
runtime implementation.

Current Phase 1 scope includes:
- scalar values (`null`, `bool`, integers, floats)
- string and binary buffers
- array and map containers
- deep copy and recursive cleanup
- optional backend dispatch through `ABackend`
- Unity-based tests with a 100% coverage gate on the core source files

## Status

Phase 1 of the C core is implemented and verified with:
- `task build`
- `task test`
- `task check`

Planned later phases include serialization backends and TypeScript bindings. The detailed project
plan lives in `SPECIFICATION.md`.

## Repository Layout

- `include/anyvar.h` — public C API
- `src/anyvar-runtime.c` — scalar, buffer, lifecycle, allocator, and backend bridge logic
- `src/anyvar-containers.c` — array and map container implementation
- `tests/` — Unity test suite
- `vendor/unity/` — vendored Unity sources
- `Taskfile.yml` — primary project entrypoint for common workflows
- `CMakeLists.txt` — build system definition

## Requirements

- CMake 3.25+
- a C11 compiler
- [Task](https://taskfile.dev/) for the standard project workflow
- LLVM tools on macOS for the configured `clang-format` and `clang-tidy` tasks

## Quick Start

List available tasks:

```bash
task
```

Configure and build:

```bash
task build
```

Run the initial test suite:

```bash
task test
```

Run the full verification gate:

```bash
task check
```

## Build Variants

The default build uses the `default` CMake preset and produces the `anyvar` library plus the
`anyvar-tests` runner when testing is enabled.

Additional project tasks:

- `task build:wasm` — build a WASM-oriented variant
- `task build:embedded` — build an embedded-oriented variant with no heap, no internal locking,
  and packed layout
- `task test:coverage` — run coverage instrumentation and enforce the 100% line coverage gate
- `task clean` — remove build artifacts

You can also use CMake directly:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## Compile-Time Options

The CMake build exposes these options:

- `AVAR_NO_HEAP` — disable heap allocation paths
- `AVAR_CUSTOM_ALLOC` — enable custom allocator hooks
- `AVAR_NO_THREAD_SAFE` — disable the internal mutex
- `AVAR_PACKED` — request a packed `AVar` layout
- `AVAR_NO_MAP` — disable map support
- `ENABLE_COVERAGE` — enable coverage instrumentation
- `ENABLE_SANITIZERS` — enable address/undefined-behavior sanitizers in non-coverage builds

These options map to the public preprocessor flags documented in `include/anyvar.h`.

## Public API Overview

The public interface is declared in `include/anyvar.h`.

### Core types

- `AVar` — the tagged value container
- `AVarType` — 32-bit type/flag field
- `ABackend` — optional backend dispatch vtable
- `AVarAllocator` — custom allocation hooks

### Supported value kinds

- `A_NULL`
- `A_BOOL`
- `A_INT64`
- `A_UINT64`
- `A_INT32`
- `A_UINT32`
- `A_DOUBLE`
- `A_FLOAT32`
- `A_STRING`
- `A_BINARY`
- `A_ARRAY`
- `A_MAP`

### Type helpers

Macros are provided for inspecting and classifying values:

- `A_BASE_TYPE`
- `A_IS_BACKEND`
- `A_IS_NUMERIC`
- `A_IS_INTEGER`
- `A_IS_FLOAT`
- `A_IS_BUFFER`
- `A_IS_CONTAINER`
- `A_IS_UNSIGNED`
- `A_IS_32BIT_INT`

### Error codes

Public operations return `AVAR_OK` or a negative error code such as:

- `AVAR_ERR_TYPE`
- `AVAR_ERR_NULL`
- `AVAR_ERR_OOM`
- `AVAR_ERR_BOUNDS`
- `AVAR_ERR_KEY`
- `AVAR_ERR_BACKEND`
- `AVAR_ERR_ARGS`

## Common API Groups

### Scalar and buffer operations

- `aVar_setNull`
- `aVar_setBool`
- `aVar_setI64`
- `aVar_setU64`
- `aVar_setI32`
- `aVar_setU32`
- `aVar_setDouble`
- `aVar_setFloat32`
- `aVar_setString`
- `aVar_setStringLen`
- `aVar_setBinary`

Readers:

- `aVar_type`
- `aVar_asBool`
- `aVar_asI64`
- `aVar_asU64`
- `aVar_asI32`
- `aVar_asU32`
- `aVar_asDouble`
- `aVar_asFloat32`
- `aVar_asString`
- `aVar_asBinary`

Lifecycle:

- `aVar_clear`
- `aVar_copy`
- `aVar_convert`

### Container operations

Arrays:

- `aVar_arrayInit`
- `aVar_arrayInitFixed`
- `aVar_arrayLen`
- `aVar_arrayPush`
- `aVar_arrayGet`

Maps:

- `aVar_mapInit`
- `aVar_mapInitFixed`
- `aVar_mapLen`
- `aVar_mapSet`
- `aVar_mapGet`

### Allocator hooks

When built with `A_CUSTOM_ALLOC`, allocator hooks can be configured with:

- `aVar_setAllocator`
- `aVar_resetAllocator`

## Usage Examples

### Scalars

```c
#include "anyvar.h"

int main(void)
{
    AVar value = {0};

    if (aVar_setI64(&value, 42) != AVAR_OK) {
        return 1;
    }

    if (aVar_type(&value) != A_INT64) {
        return 1;
    }

    if (aVar_asI64(&value) != 42) {
        return 1;
    }

    return aVar_clear(&value) == AVAR_OK ? 0 : 1;
}
```

### Borrowed and owned strings

`aVar_setString()` supports both borrowed and copied storage:

```c
#include "anyvar.h"

int main(void)
{
    AVar borrowed = {0};
    AVar owned = {0};

    if (aVar_setString(&borrowed, "hello", false) != AVAR_OK) {
        return 1;
    }

    if (aVar_setString(&owned, "world", true) != AVAR_OK) {
        aVar_clear(&borrowed);
        return 1;
    }

    aVar_clear(&borrowed);
    aVar_clear(&owned);
    return 0;
}
```

### Arrays

```c
#include "anyvar.h"

int main(void)
{
    AVar array = {0};
    AVar item = {0};
    AVar out = {0};

    if (aVar_arrayInit(&array, 1u) != AVAR_OK) {
        return 1;
    }

    aVar_setI32(&item, 7);
    aVar_arrayPush(&array, &item);
    aVar_arrayGet(&array, 0u, &out);

    aVar_clear(&out);
    aVar_clear(&item);
    aVar_clear(&array);
    return 0;
}
```

### Maps

```c
#include "anyvar.h"

int main(void)
{
    AVar map = {0};
    AVar value = {0};
    AVar out = {0};

    if (aVar_mapInit(&map, 1u) != AVAR_OK) {
        return 1;
    }

    aVar_setU32(&value, 42u);
    aVar_mapSet(&map, "answer", &value, true);
    aVar_mapGet(&map, "answer", &out);

    aVar_clear(&out);
    aVar_clear(&value);
    aVar_clear(&map);
    return 0;
}
```

## Ownership Model

- Scalars are stored inline and do not allocate.
- `aVar_setString(..., false)` and `aVar_setBinary(..., false)` create borrowed views.
- `aVar_setString(..., true)` and `aVar_setBinary(..., true)` copy data and mark it owned.
- Arrays and maps own their child values recursively.
- `aVar_copy` performs a deep copy.
- `aVar_clear` releases owned buffers and owned container contents recursively.

## Backend Dispatch

`AVar` can also operate in backend mode through an `ABackend` vtable. Phase 1 exposes the
backend interface and conversion hook:

- `aVar_convert`
- `AVAR_CBOR_BACKEND`
- `AVAR_JSON_BACKEND`

The serialization backends themselves are planned for a later phase; the current implementation
focuses on the native core runtime and the backend bridge surface.

## Testing

The project uses Unity for the C test suite.

Primary commands:

- `task test`
- `task test:coverage`
- `task check`

The current gate enforces 100% filtered line coverage on the core source files under `src/`.

## Integration Notes

To consume AnyVar from another CMake project, either:

- add this repository as a subdirectory and link `anyvar::anyvar`, or
- build/install it and include `anyvar.h` plus the resulting library artifact

The library target defined by this project is:

- `anyvar`
- alias: `anyvar::anyvar`

## Project Documents

- `PROJECT.md` — project rules and workflow expectations
- `PRD.md` — product requirements
- `SPECIFICATION.md` — implementation plan and phased scope
- `CHANGELOG.md` — notable changes

## License

This repository currently documents the public header as `MIT OR Apache-2.0`.
