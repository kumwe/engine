# Narrow, source-pinned PCRE2 dependency for the existing bounded pattern contract.
# These object files are incorporated into the Engine archive; no system PCRE is selected.
set(PCRE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pcre2")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/pcre2-source.json" PCRE_LOCK)
string(JSON PCRE_FILE_COUNT LENGTH "${PCRE_LOCK}" files)
math(EXPR PCRE_LAST_FILE "${PCRE_FILE_COUNT} - 1")
set(PCRE_EXPECTED_FILES)
foreach(PCRE_INDEX RANGE 0 ${PCRE_LAST_FILE})
  string(JSON PCRE_FILE MEMBER "${PCRE_LOCK}" files ${PCRE_INDEX})
  string(JSON PCRE_EXPECTED GET "${PCRE_LOCK}" files "${PCRE_FILE}")
  file(SHA256 "${PCRE_SOURCE}/${PCRE_FILE}" PCRE_ACTUAL)
  if(NOT PCRE_EXPECTED STREQUAL PCRE_ACTUAL)
    message(FATAL_ERROR "Pinned PCRE2 source differs: ${PCRE_FILE}")
  endif()
  list(APPEND PCRE_EXPECTED_FILES "${PCRE_FILE}")
endforeach()
file(GLOB_RECURSE PCRE_ACTUAL_FILES LIST_DIRECTORIES FALSE RELATIVE "${PCRE_SOURCE}" "${PCRE_SOURCE}/*")
list(SORT PCRE_EXPECTED_FILES)
list(SORT PCRE_ACTUAL_FILES)
if(NOT PCRE_EXPECTED_FILES STREQUAL PCRE_ACTUAL_FILES)
  message(FATAL_ERROR "Unexpected or missing PCRE2 source files")
endif()
set(PCRE_BINARY "${CMAKE_CURRENT_BINARY_DIR}/pcre2")
file(MAKE_DIRECTORY "${PCRE_BINARY}")
configure_file("${PCRE_SOURCE}/src/config.h.generic" "${PCRE_BINARY}/config.h" COPYONLY)
configure_file("${PCRE_SOURCE}/src/pcre2.h.generic" "${PCRE_BINARY}/pcre2.h" COPYONLY)
configure_file("${PCRE_SOURCE}/src/pcre2_chartables.c.dist" "${PCRE_BINARY}/pcre2_chartables.c" COPYONLY)
set(PCRE_SOURCES
  "${PCRE_SOURCE}/src/pcre2_auto_possess.c"
  "${PCRE_SOURCE}/src/pcre2_compile.c"
  "${PCRE_SOURCE}/src/pcre2_config.c"
  "${PCRE_SOURCE}/src/pcre2_context.c"
  "${PCRE_SOURCE}/src/pcre2_convert.c"
  "${PCRE_SOURCE}/src/pcre2_dfa_match.c"
  "${PCRE_SOURCE}/src/pcre2_error.c"
  "${PCRE_SOURCE}/src/pcre2_extuni.c"
  "${PCRE_SOURCE}/src/pcre2_find_bracket.c"
  "${PCRE_SOURCE}/src/pcre2_jit_compile.c"
  "${PCRE_SOURCE}/src/pcre2_maketables.c"
  "${PCRE_SOURCE}/src/pcre2_match.c"
  "${PCRE_SOURCE}/src/pcre2_match_data.c"
  "${PCRE_SOURCE}/src/pcre2_newline.c"
  "${PCRE_SOURCE}/src/pcre2_ord2utf.c"
  "${PCRE_SOURCE}/src/pcre2_pattern_info.c"
  "${PCRE_SOURCE}/src/pcre2_script_run.c"
  "${PCRE_SOURCE}/src/pcre2_serialize.c"
  "${PCRE_SOURCE}/src/pcre2_string_utils.c"
  "${PCRE_SOURCE}/src/pcre2_study.c"
  "${PCRE_SOURCE}/src/pcre2_substitute.c"
  "${PCRE_SOURCE}/src/pcre2_substring.c"
  "${PCRE_SOURCE}/src/pcre2_tables.c"
  "${PCRE_SOURCE}/src/pcre2_ucd.c"
  "${PCRE_SOURCE}/src/pcre2_valid_utf.c"
  "${PCRE_SOURCE}/src/pcre2_xclass.c"
  "${PCRE_BINARY}/pcre2_chartables.c"
)
add_library(kumwe_pcre2 OBJECT ${PCRE_SOURCES})
set_target_properties(kumwe_pcre2 PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
target_include_directories(kumwe_pcre2 PRIVATE "${PCRE_SOURCE}/src" "${PCRE_BINARY}")
target_compile_definitions(kumwe_pcre2 PRIVATE HAVE_CONFIG_H PCRE2_STATIC PCRE2_CODE_UNIT_WIDTH=8 SUPPORT_PCRE2_8 SUPPORT_UNICODE SUPPORT_JIT HAVE_MEMMOVE HAVE_STDINT_H HAVE_INTTYPES_H HAVE_STDLIB_H HAVE_STRING_H HAVE_STRERROR)
target_compile_options(kumwe_pcre2 PRIVATE -fstack-protector-strong)
if(KUMWE_ENGINE_SANITIZERS)
  target_compile_options(kumwe_pcre2 PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
if(KUMWE_ENGINE_FUZZ)
  target_compile_options(kumwe_pcre2 PRIVATE -fsanitize=fuzzer-no-link)
endif()
