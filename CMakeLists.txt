cmake_minimum_required(VERSION 3.13)

project(tree-sitter-nova
        VERSION "0.1.0"
        DESCRIPTION "NOVA (Notation for Objects, Values & Agents) grammar for tree-sitter"
        HOMEPAGE_URL "https://github.com/novafile/tree-sitter-nova"
        LANGUAGES C)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(TREE_SITTER_REUSE_ALLOCATOR "Reuse the library allocator" OFF)

set(TREE_SITTER_ABI_VERSION 15 CACHE STRING "Tree-sitter ABI version")
if(NOT ${TREE_SITTER_ABI_VERSION} MATCHES "^[0-9]+$")
    unset(TREE_SITTER_ABI_VERSION CACHE)
    message(FATAL_ERROR "TREE_SITTER_ABI_VERSION must be an integer")
endif()

include(GNUInstallDirs)

find_program(TREE_SITTER_CLI tree-sitter DOC "Tree-sitter CLI" REQUIRED)

add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/src/grammar.json"
                          "${CMAKE_CURRENT_SOURCE_DIR}/src/node-types.json"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/grammar.js"
                   COMMAND "${TREE_SITTER_CLI}" generate grammar.js --no-parser
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   COMMENT "Generating grammar.json")

add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/src/parser.c"
                   BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/src/tree_sitter/parser.h"
                              "${CMAKE_CURRENT_SOURCE_DIR}/src/tree_sitter/alloc.h"
                              "${CMAKE_CURRENT_SOURCE_DIR}/src/tree_sitter/array.h"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/grammar.json"
                   COMMAND "${TREE_SITTER_CLI}" generate src/grammar.json
                            --abi=${TREE_SITTER_ABI_VERSION}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   COMMENT "Generating parser.c")

add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/schema/src/grammar.json"
                          "${CMAKE_CURRENT_SOURCE_DIR}/schema/src/node-types.json"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/schema/grammar.js"
                   COMMAND "${TREE_SITTER_CLI}" generate grammar.js --no-parser
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/schema"
                   COMMENT "Generating schema/src/grammar.json")

add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/schema/src/parser.c"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/schema/src/grammar.json"
                   COMMAND "${TREE_SITTER_CLI}" generate src/grammar.json
                            --abi=${TREE_SITTER_ABI_VERSION}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/schema"
                   COMMENT "Generating schema/src/parser.c")

# One library with both languages: tree_sitter_nova() and
# tree_sitter_nova_schema().
add_library(tree-sitter-nova src/parser.c src/scanner.c schema/src/parser.c schema/src/scanner.c)
target_include_directories(tree-sitter-nova
                           PRIVATE src
                           INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bindings/c>
                                     $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_definitions(tree-sitter-nova PRIVATE
                           $<$<BOOL:${TREE_SITTER_REUSE_ALLOCATOR}>:TREE_SITTER_REUSE_ALLOCATOR>
                           $<$<CONFIG:Debug>:TREE_SITTER_DEBUG>)

set_target_properties(tree-sitter-nova
                      PROPERTIES
                      C_STANDARD 11
                      POSITION_INDEPENDENT_CODE ON
                      SOVERSION "${TREE_SITTER_ABI_VERSION}.${PROJECT_VERSION_MAJOR}"
                      DEFINE_SYMBOL "")

configure_file(bindings/c/tree-sitter-nova.pc.in
               "${CMAKE_CURRENT_BINARY_DIR}/tree-sitter-nova.pc" @ONLY)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bindings/c/tree_sitter"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN "*.h")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tree-sitter-nova.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
install(TARGETS tree-sitter-nova
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

file(GLOB QUERIES queries/*.scm)
install(FILES ${QUERIES}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/tree-sitter/queries/nova")
file(GLOB SCHEMA_QUERIES schema/queries/*.scm)
install(FILES ${SCHEMA_QUERIES}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/tree-sitter/queries/nova_schema")

add_custom_target(ts-test "${TREE_SITTER_CLI}" test
                  COMMAND "${TREE_SITTER_CLI}" test -p schema
                  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                  COMMENT "tree-sitter test")
