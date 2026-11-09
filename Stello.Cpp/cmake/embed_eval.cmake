if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "embed_eval.cmake requires -DINPUT=... and -DOUTPUT=...")
endif()

get_filename_component(INPUT_NAME "${INPUT}" NAME)
file(READ "${INPUT}" HEX_DATA HEX)
string(LENGTH "${HEX_DATA}" HEX_LEN)
math(EXPR BYTE_LEN "${HEX_LEN} / 2")

# Insert a newline every 16 bytes (32 hex digits), then expand every hex pair to 0xNN,.
string(REGEX REPLACE "([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f])" "\\1\n" HEX_LINES "${HEX_DATA}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BODY "${HEX_LINES}")

file(WRITE "${OUTPUT}" "// Auto-generated from ${INPUT_NAME}; do not edit.\n#include <cstdint>\n#include <span>\n\nnamespace stello::eval::detail {\n\nalignas(16) static constexpr uint8_t kEmbeddedEvalData[] = {\n${BODY}\n};\n\nstatic_assert(sizeof(kEmbeddedEvalData) == ${BYTE_LEN}u);\n\nstd::span<const uint8_t> embedded_eval_bytes() noexcept {\n    return {kEmbeddedEvalData, sizeof(kEmbeddedEvalData)};\n}\n\n} // namespace stello::eval::detail\n")
