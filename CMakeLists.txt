cmake_minimum_required(VERSION 3.20)

# ============================================================
# Project: Chista Asabru - Enterprise-Grade Database Proxy
# ============================================================
project(Chista_Asabru
    VERSION 1.1.0
    DESCRIPTION "A protocol-aware, high-performance database proxy"
    LANGUAGES CXX
)

# ─── Policies ───────────────────────────────────────────────
cmake_policy(SET CMP0054 NEW)   # Only interpret if() args as vars/keywords when unquoted
cmake_policy(SET CMP0074 NEW)   # find_package uses <PackageName>_ROOT
cmake_policy(SET CMP0091 NEW)   # MSVC runtime library flags

# ─── Build type default ─────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Build type not set — defaulting to Release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ─── Compile commands (clangd / IDE) ────────────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Compiler hardening flags (GCC / Clang) ─────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wformat=2 -Wformat-security
        -Wno-unused-parameter
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O2)
    endif()
endif()

# ─── Coverage (opt-in) ──────────────────────────────────────
option(ENABLE_COVERAGE "Enable gcov code-coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
    find_program(GCOV_PATH gcov)
    if(NOT GCOV_PATH)
        message(FATAL_ERROR "ENABLE_COVERAGE=ON but gcov was not found")
    endif()
    message(STATUS "Coverage enabled — gcov: ${GCOV_PATH}")
    add_compile_options(--coverage -O0 -g -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif()

# ─── Sub-directories ────────────────────────────────────────
add_subdirectory(src)

# ─── Tests ──────────────────────────────────────────────────
option(GTEST_ENABLED "Build unit-test suite" OFF)
if(GTEST_ENABLED)
    enable_testing()
    add_subdirectory(test)
endif()

# ─── Install rules ──────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS Chista_Asabru
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES config.xml.sample
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/chista-asabru
    RENAME config.xml
)

# ─── CPack packaging ────────────────────────────────────────
set(CPACK_PACKAGE_NAME        "chista-asabru")
set(CPACK_PACKAGE_VERSION     "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_GENERATOR           "TGZ;DEB")
include(CPack)
