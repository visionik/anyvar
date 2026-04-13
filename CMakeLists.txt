cmake_minimum_required(VERSION 3.25)

project(anyvar VERSION 0.1.0 LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AVAR_NO_HEAP "Disable heap allocation in AnyVar" OFF)
option(AVAR_CUSTOM_ALLOC "Enable custom allocator hooks in AnyVar" OFF)
option(AVAR_NO_THREAD_SAFE "Disable internal thread safety in AnyVar" OFF)
option(AVAR_PACKED "Enable packed layout for AVar" OFF)
option(AVAR_NO_MAP "Disable map support in AnyVar" OFF)
option(ENABLE_COVERAGE "Enable coverage instrumentation" OFF)
option(ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" ON)

set(AVAR_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion
    -Wshadow
    -Wformat=2
)

function(avar_apply_common_flags target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target_name} PRIVATE ${AVAR_WARNING_FLAGS})
    endif()

    if(ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
        target_link_options(${target_name} PRIVATE --coverage)
    endif()

    if(ENABLE_SANITIZERS AND NOT WIN32 AND NOT ENABLE_COVERAGE
       AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined)
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

add_library(anyvar src/anyvar-runtime.c src/anyvar-containers.c)
add_library(anyvar::anyvar ALIAS anyvar)

target_include_directories(
    anyvar
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(
    anyvar
    PUBLIC
        $<$<BOOL:${AVAR_NO_HEAP}>:A_NO_HEAP>
        $<$<BOOL:${AVAR_CUSTOM_ALLOC}>:A_CUSTOM_ALLOC>
        $<$<BOOL:${AVAR_NO_THREAD_SAFE}>:A_NO_THREAD_SAFE>
        $<$<BOOL:${AVAR_PACKED}>:A_PACKED>
        $<$<BOOL:${AVAR_NO_MAP}>:A_NO_MAP>
    PRIVATE
        $<$<NOT:$<BOOL:${WIN32}>>:_POSIX_C_SOURCE=200809L>
)

avar_apply_common_flags(anyvar)

if(NOT AVAR_NO_THREAD_SAFE)
    find_package(Threads REQUIRED)
    target_link_libraries(anyvar PUBLIC Threads::Threads)
endif()

install(
    TARGETS anyvar
    EXPORT anyvar-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES include/anyvar.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(BUILD_TESTING)
    if(NOT MSVC)
        set_source_files_properties(
            vendor/unity/unity.c
            PROPERTIES COMPILE_OPTIONS "-Wno-float-conversion"
        )
    endif()
    add_executable(
        anyvar-tests
        vendor/unity/unity.c
        tests/test-main.c
        tests/test-scalars.c
        tests/test-buffers.c
        tests/test-containers.c
        tests/test-errors.c
    )

    target_include_directories(
        anyvar-tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity
    )

    target_link_libraries(anyvar-tests PRIVATE anyvar)
    target_compile_definitions(anyvar-tests PRIVATE UNITY_INCLUDE_DOUBLE)
    avar_apply_common_flags(anyvar-tests)

    add_test(NAME anyvar-tests COMMAND anyvar-tests)
endif()
