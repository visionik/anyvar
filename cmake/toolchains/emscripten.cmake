if(DEFINED ENV{EMSCRIPTEN})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{EMSCRIPTEN}/cmake/Modules/Platform/Emscripten.cmake")
elseif(DEFINED ENV{EMSDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
else()
    message(
        FATAL_ERROR
        "Emscripten toolchain not found. Use emcmake or set EMSCRIPTEN/EMSDK."
    )
endif()
