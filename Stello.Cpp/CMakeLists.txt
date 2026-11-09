cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(StelloCpp VERSION 3.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STELLO_ABORT_EXCEPTIONS "Use C++ exception unwinding for Abort instead of Result<T, Abort>" OFF)
option(STELLO_SANITIZE "Enable ASan + UBSan runtime checks" OFF)
option(STELLO_BUILD_TESTS "Build the Stello C++ test executable" ON)

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
target_compile_definitions(stello_core PUBLIC
    STELLO_ABORT_EXCEPTIONS=$<BOOL:${STELLO_ABORT_EXCEPTIONS}>
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT EMSCRIPTEN)
    set_source_files_properties(src/eval/patterns_bmi2.cpp PROPERTIES
        COMPILE_OPTIONS "-mbmi2"
    )
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(stello_core PUBLIC Threads::Threads)
endif()

add_executable(stello-cpp
    src/cli/main.cpp
    src/cli/bench.cpp
)
target_link_libraries(stello-cpp PRIVATE stello_core stello_warnings)

enable_testing()
if(STELLO_BUILD_TESTS)
    add_executable(stello_tests
        tests/test_main.cpp
        tests/parity_tests.cpp
        tests/search_tests.cpp
        tests/threads_tests.cpp
        src/cli/bench.cpp
    )
    target_include_directories(stello_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(stello_tests PRIVATE stello_core stello_warnings)
    target_compile_definitions(stello_tests PRIVATE
        STELLO_REPO_ROOT="${STELLO_REPO_ROOT}"
    )

    add_test(NAME stello_tests COMMAND stello_tests)
    add_test(NAME stello_cli_version COMMAND stello-cpp version)
endif()

if(WIN32 AND STELLO_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
        OUTPUT_VARIABLE STELLO_CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(STELLO_ASAN_DLL "${STELLO_CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.dll")
    if(EXISTS "${STELLO_ASAN_DLL}")
        foreach(stello_tgt IN ITEMS stello-cpp stello_tests)
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
