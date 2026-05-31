cmake_minimum_required(VERSION 3.20)

# ─── Language standard ───────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CMakePrintHelpers)
include(GNUInstallDirs)

# ─── Preprocessor definitions ───────────────────────────────
# Resolve optional paths from environment variables at build time.
# Fall back to empty strings so the binary remains buildable without them.
set(PLUGINS_FOLDER_PATH_DEFAULT "" CACHE STRING "Default plugin folder path (overridable via env PLUGINS_FOLDER_PATH)")
set(CONFIG_FILE_DEFAULT     ""   CACHE STRING "Default config file path")
set(CONFIG_FILE_URL_DEFAULT ""   CACHE STRING "Default config file URL")

add_compile_definitions(
    PLUGINS_FOLDER_PATH="$ENV{PLUGINS_FOLDER_PATH}"
    CONFIG_FILE="$ENV{CONFIG_FILE}"
    CONFIG_FILE_URL="$ENV{CONFIG_FILE_URL}"
)

# ─── Build-type-aware macros ─────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(CHISTA_DEBUG=1)
else()
    add_compile_definitions(CHISTA_DEBUG=0)
endif()

# ─── Threading ───────────────────────────────────────────────
find_package(Threads REQUIRED)

# ─── CPM (package manager) ──────────────────────────────────
include(../cmake/CPM.cmake)

# ─── OpenSSL ─────────────────────────────────────────────────
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL version : ${OPENSSL_VERSION}")
message(STATUS "OpenSSL include : ${OPENSSL_INCLUDE_DIR}")

# ─── Python (optional — required for the /python endpoint) ──
find_package(Python COMPONENTS Interpreter Development REQUIRED)
message(STATUS "Python version  : ${Python_VERSION}")
include_directories(SYSTEM ${Python_INCLUDE_DIRS})

# ─── SQLite3 ─────────────────────────────────────────────────
find_package(SQLite3 REQUIRED)
include_directories(SYSTEM ${SQLite3_INCLUDE_DIRS})

# ─── asabru-commons ─────────────────────────────────────────
if(ASABRU_COMMONS_BUILD STREQUAL "GIT_TAG")
    CPMAddPackage(
        NAME asabru_commons
        GITHUB_REPOSITORY ChistaDATA/asabru-commons
        GIT_TAG ${ASABRU_COMMONS_GIT_TAG}
    )
elseif(ASABRU_COMMONS_BUILD STREQUAL "LOCAL_DIR")
    CPMAddPackage(
        NAME asabru_commons
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/asabru-commons/src
    )
else()
    CPMAddPackage(
        NAME asabru_commons
        GITHUB_REPOSITORY ChistaDATA/asabru-commons
        VERSION 0.0.3
    )
endif()

# ─── asabru-engine ──────────────────────────────────────────
if(ASABRU_ENGINE_BUILD STREQUAL "GIT_TAG")
    CPMAddPackage(
        NAME asabru_engine
        GITHUB_REPOSITORY ChistaDATA/asabru-engine
        GIT_TAG ${ASABRU_ENGINE_GIT_TAG}
    )
elseif(ASABRU_ENGINE_BUILD STREQUAL "LOCAL_DIR")
    CPMAddPackage(
        NAME asabru_engine
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/asabru-engine/src
    )
else()
    CPMAddPackage(
        NAME asabru_engine
        GITHUB_REPOSITORY ChistaDATA/asabru-engine
        VERSION 0.0.2
    )
endif()

# ─── asabru-parsers ─────────────────────────────────────────
if(ASABRU_PARSERS_BUILD STREQUAL "GIT_TAG")
    CPMAddPackage(
        NAME asabru_parsers
        GITHUB_REPOSITORY ChistaDATA/asabru-parsers
        GIT_TAG ${ASABRU_PARSERS_GIT_TAG}
    )
elseif(ASABRU_PARSERS_BUILD STREQUAL "LOCAL_DIR")
    CPMAddPackage(
        NAME asabru_parsers
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/asabru-parsers/src
    )
else()
    CPMAddPackage(
        NAME asabru_parsers
        GITHUB_REPOSITORY ChistaDATA/asabru-parsers
        VERSION 0.0.1
    )
endif()

# ─── Pipeline source files ───────────────────────────────────
file(GLOB_RECURSE PIPELINE_FILES "./pipelines/*.cpp")

# ─── UI / config assets ──────────────────────────────────────
# Only copy if the source exists (avoids hard failures in CI without the UI submodule)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../lib/asabru-ui/build")
    file(COPY ../lib/asabru-ui/build/ DESTINATION ../public)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../config.xml")
    file(COPY ../config.xml DESTINATION ..)
endif()

# ─── Executable ─────────────────────────────────────────────
add_executable(Chista_Asabru
    main.h
    main.cpp

    config/ConfigParser.cpp
    config/ApiGatewayConfigParser.cpp
    config/ConfigSingleton.cpp
    config/PipelineFactory.cpp
    config/LoadBalancerFactory.cpp

    ${PIPELINE_FILES}
)

# Enforce strict compile options on our own code only
target_compile_options(Chista_Asabru PRIVATE
    -Wall -Wextra -Wpedantic
    -Wformat=2 -Wformat-security
    -Wno-unused-parameter
    -fstack-protector-strong
)

target_compile_definitions(Chista_Asabru PRIVATE
    _FORTIFY_SOURCE=2
)

# ─── Link libraries ─────────────────────────────────────────
target_link_libraries(Chista_Asabru
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ${Python_LIBRARIES}
        SQLite::SQLite3
        Threads::Threads
        asabru_commons
        asabru_engine
        asabru_parsers
        ${CMAKE_DL_LIBS}
)

# ─── Version header (auto-generated) ─────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)
target_include_directories(Chista_Asabru PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# ─── Install ─────────────────────────────────────────────────
install(TARGETS Chista_Asabru
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
