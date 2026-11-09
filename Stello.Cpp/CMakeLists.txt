cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(StelloCpp VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STELLO_ABORT_EXCEPTIONS "Use C++ exception unwinding for Abort instead of Result<T, Abort>" ON)
option(STELLO_SANITIZE "Enable ASan + UBSan runtime checks" OFF)
option(STELLO_BUILD_SHARED "Build the stello_engine shared C ABI library" ON)
option(STELLO_BUILD_CLI "Build the stello-cpp CLI benchmark executable" ON)
option(STELLO_BUILD_TESTS "Build the Stello C++ test executable" ON)
option(STELLO_BUILD_WASM "Build the WebAssembly engine module (Emscripten)" OFF)
option(STELLO_WASM_THREADS "Build multi-threaded WebAssembly engine with pthreads" OFF)
set(STELLO_WASM_OUT_DIR "" CACHE PATH "Optional output directory for stello_wasm.js and stello_wasm_bg.wasm")

if(WIN32)
    if(STELLO_SANITIZE)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

get_filename_component(STELLO_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(STELLO_EVAL_BIN "${STELLO_REPO_ROOT}/Stello.Net/Book/stello-eval.bin" CACHE FILEPATH "Path to embedded stello-eval.bin")
if(NOT EXISTS "${STELLO_EVAL_BIN}")
    message(FATAL_ERROR "Cannot find embedded evaluation weights at: ${STELLO_EVAL_BIN}")
endif()

set(STELLO_EMBEDDED_EVAL_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_eval.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_custom_command(
    OUTPUT "${STELLO_EMBEDDED_EVAL_CPP}"
    COMMAND "${CMAKE_COMMAND}"
        "-DINPUT=${STELLO_EVAL_BIN}"
        "-DOUTPUT=${STELLO_EMBEDDED_EVAL_CPP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_eval.cmake"
    DEPENDS "${STELLO_EVAL_BIN}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_eval.cmake"
    VERBATIM
    COMMENT "Generating embedded_eval.cpp from stello-eval.bin"
)

add_library(stello_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(stello_warnings INTERFACE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(stello_warnings INTERFACE /W4 /permissive- /utf-8 /EHsc)
endif()

add_library(stello_core STATIC
    "${STELLO_EMBEDDED_EVAL_CPP}"
    src/core/board.cpp
    src/core/cpu.cpp
    src/eval/gzip.cpp
    src/eval/patterns.cpp
    src/eval/patterns_bmi2.cpp
    src/eval/weights.cpp
    src/endgame/last.cpp
    src/endgame/search.cpp
    src/endgame/stable.cpp
    src/search/engine.cpp
    src/search/ordering.cpp
    src/search/table.cpp
    src/search/ybwc.cpp
)
target_include_directories(stello_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(stello_core PRIVATE stello_warnings)
set_target_properties(stello_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(stello_core PUBLIC
    STELLO_ABORT_EXCEPTIONS=$<BOOL:${STELLO_ABORT_EXCEPTIONS}>
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT EMSCRIPTEN)
    set_source_files_properties(src/eval/patterns_bmi2.cpp PROPERTIES
        COMPILE_OPTIONS "-mbmi2"
    )
endif()
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64" AND NOT EMSCRIPTEN)
    # MSVC <atomic> tags _Atomic_storage with gnu::target("cx16"); without -mcx16 Clang keeps
    # 1-byte relaxed loads out-of-line on every leaf evaluation instead of folding to movb.
    target_compile_options(stello_core PUBLIC -mcx16)
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(stello_core PUBLIC Threads::Threads)
endif()

if(STELLO_BUILD_SHARED AND NOT EMSCRIPTEN)
    add_library(stello_engine SHARED
        src/ffi/stello_ffi.cpp
    )
    set_target_properties(stello_engine PROPERTIES
        OUTPUT_NAME "stello_engine"
    )
    target_link_libraries(stello_engine PRIVATE stello_core stello_warnings)
    target_compile_definitions(stello_engine PRIVATE STELLO_BUILDING_DLL)
endif()

if(STELLO_BUILD_CLI AND NOT EMSCRIPTEN)
    add_executable(stello-cpp
        src/cli/main.cpp
        src/cli/bench.cpp
    )
    target_link_libraries(stello-cpp PRIVATE stello_core stello_warnings)
endif()

if(EMSCRIPTEN AND STELLO_BUILD_WASM)
    add_executable(stello_wasm
        src/wasm/protocol.cpp
        src/wasm/threads.cpp
        src/wasm/wasm_exports.cpp
    )
    set_target_properties(stello_wasm PROPERTIES
        OUTPUT_NAME "stello_wasm_bg"
        SUFFIX ".wasm"
    )
    target_link_libraries(stello_wasm PRIVATE stello_core stello_warnings)
    target_compile_options(stello_core PUBLIC -fwasm-exceptions)

    set(_stello_wasm_exports
        "-Wl,--export=_initialize"
        "-Wl,--export=stello_wasm_version"
        "-Wl,--export=stello_wasm_threaded"
        "-Wl,--export=stello_wasm_alloc"
        "-Wl,--export=stello_wasm_free"
        "-Wl,--export=stello_wasm_result_ptr"
        "-Wl,--export=stello_wasm_result_len"
        "-Wl,--export=stello_wasm_init"
        "-Wl,--export=stello_wasm_search"
        "-Wl,--export=stello_wasm_idle_threads"
        "-Wl,--export=stello_wasm_pool_worker"
    )

    if(STELLO_WASM_THREADS)
        target_compile_options(stello_core PUBLIC -pthread)
        target_compile_options(stello_wasm PRIVATE -pthread)
        target_link_options(stello_wasm PRIVATE
            -pthread
            -fwasm-exceptions
            -sSTANDALONE_WASM=1
            --no-entry
            -sALLOW_MEMORY_GROWTH=1
            -sMAXIMUM_MEMORY=2GB
            -sINITIAL_MEMORY=32MB
            -Wno-pthreads-mem-growth
            ${_stello_wasm_exports}
            "-Wl,--export=stello_wasm_record_main"
            "-Wl,--export=stello_wasm_boot_lock_ptr"
            "-Wl,--export=stello_wasm_boot_stack_top"
            "-Wl,--export=stello_wasm_worker_prepare"
        )
    else()
        target_link_options(stello_wasm PRIVATE
            -fwasm-exceptions
            -sSTANDALONE_WASM=1
            --no-entry
            -sALLOW_MEMORY_GROWTH=1
            -sMAXIMUM_MEMORY=2GB
            -sINITIAL_MEMORY=16MB
            ${_stello_wasm_exports}
        )
    endif()

    if(NOT "${STELLO_WASM_OUT_DIR}" STREQUAL "")
        get_filename_component(_stello_wasm_out "${STELLO_WASM_OUT_DIR}" ABSOLUTE)
        file(MAKE_DIRECTORY "${_stello_wasm_out}")
        add_custom_command(
            OUTPUT "${_stello_wasm_out}/stello_wasm_bg.wasm"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:stello_wasm>"
                "${_stello_wasm_out}/stello_wasm_bg.wasm"
            DEPENDS stello_wasm "$<TARGET_FILE:stello_wasm>"
            VERBATIM
        )
        add_custom_command(
            OUTPUT "${_stello_wasm_out}/stello_wasm.js"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/stello_wasm.js"
                "${_stello_wasm_out}/stello_wasm.js"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/stello_wasm.js"
            VERBATIM
        )
        add_custom_target(stello_wasm_stage ALL DEPENDS
            "${_stello_wasm_out}/stello_wasm_bg.wasm"
            "${_stello_wasm_out}/stello_wasm.js"
        )
    else()
        add_custom_target(stello_wasm_stage DEPENDS stello_wasm)
    endif()
endif()

enable_testing()
if(STELLO_BUILD_TESTS AND NOT EMSCRIPTEN)
    add_executable(stello_tests
        tests/test_main.cpp
        tests/parity_tests.cpp
        tests/search_tests.cpp
        tests/threads_tests.cpp
        tests/wasm_protocol_tests.cpp
        src/wasm/protocol.cpp
        src/cli/bench.cpp
    )
    target_include_directories(stello_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(stello_tests PRIVATE stello_core stello_warnings)
    target_compile_definitions(stello_tests PRIVATE
        STELLO_REPO_ROOT="${STELLO_REPO_ROOT}"
    )
    if(TARGET stello_engine)
        target_sources(stello_tests PRIVATE tests/ffi_tests.cpp)
        target_link_libraries(stello_tests PRIVATE stello_engine)
        target_compile_definitions(stello_tests PRIVATE STELLO_USE_DLL)
    endif()

    add_test(NAME stello_tests COMMAND stello_tests)
    if(TARGET stello-cpp)
        add_test(NAME stello_cli_version COMMAND stello-cpp version)
    endif()
endif()

if(WIN32 AND STELLO_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
        OUTPUT_VARIABLE STELLO_CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(STELLO_ASAN_DLL "${STELLO_CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.dll")
    if(EXISTS "${STELLO_ASAN_DLL}")
        foreach(stello_tgt IN ITEMS stello-cpp stello_tests stello_engine)
            if(TARGET ${stello_tgt})
                add_custom_command(TARGET ${stello_tgt} POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${STELLO_ASAN_DLL}"
                        "$<TARGET_FILE_DIR:${stello_tgt}>/clang_rt.asan_dynamic-x86_64.dll"
                    VERBATIM
                )
            endif()
        endforeach()
    endif()
endif()
