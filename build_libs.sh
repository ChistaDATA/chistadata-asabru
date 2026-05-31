#!/usr/bin/env bash
# build_libs.sh - Build all sub-library dependencies for Chista Asabru.
#
# Usage: ./build_libs.sh [amd64]
set -euo pipefail

log()  { printf "[build_libs] %s\n" "$*"; }
err()  { printf "[build_libs] ERROR: %s\n" "$*" >&2; }
die()  { err "$*"; exit 1; }

readonly ROOT_DIR
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TOOLCHAIN_ARG=""
if [[ "${1:-}" == "amd64" ]]; then
    TOOLCHAIN_FILE="${ROOT_DIR}/dist/ubuntu/amd64/toolchain-amd64.cmake"
    [[ -f "${TOOLCHAIN_FILE}" ]] \
        || die "Toolchain file not found: ${TOOLCHAIN_FILE}"
    TOOLCHAIN_ARG="-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}"
    log "Cross-compile toolchain: ${TOOLCHAIN_FILE}"
fi

declare -a LIB_DIRS=(
    "./lib/asabru-handlers"
    "./lib/asabru-ui"
)

build_lib() {
    local dir="$1"
    local abs_dir="${ROOT_DIR}/${dir#./}"

    [[ -d "${abs_dir}" ]] || { log "Skipping ${dir} (directory not found)"; return 0; }

    if [[ "${dir}" == "./lib/asabru-ui" ]]; then
        log "Building UI: ${dir}"
        (
            cd "${abs_dir}"
            npm ci --prefer-offline
            npm run build
        )
        return
    fi

    log "Building CMake library: ${dir}"
    (
        cd "${abs_dir}"
        rm -rf build
        mkdir -p build
        cd build

        local cmake_args=("-DCMAKE_POSITION_INDEPENDENT_CODE=ON")
        [[ -n "${TOOLCHAIN_ARG}" ]] && cmake_args+=("${TOOLCHAIN_ARG}")

        if [[ "${dir}" == *"libuv"* ]]; then
            cmake_args+=("-DBUILD_TESTING=OFF")
        fi

        if [[ "${dir}" == *"asabru-handlers"* ]]; then
            cmake_args+=(
                "-DASABRU_COMMONS_BUILD=LOCAL_DIR"
                "-DASABRU_ENGINE_BUILD=LOCAL_DIR"
                "-DASABRU_PARSERS_BUILD=LOCAL_DIR"
            )
        fi

        cmake .. "${cmake_args[@]}"
        cmake --build . --parallel "$(nproc)"
    )
}

log "Root directory : ${ROOT_DIR}"
log "Libraries      : ${LIB_DIRS[*]}"

FAILED_LIBS=()

for lib in "${LIB_DIRS[@]}"; do
    if ! build_lib "${lib}"; then
        err "Failed to build: ${lib}"
        FAILED_LIBS+=("${lib}")
    fi
done

if [[ ${#FAILED_LIBS[@]} -gt 0 ]]; then
    err "The following libraries failed to build:"
    for f in "${FAILED_LIBS[@]}"; do
        err "  - ${f}"
    done
    exit 1
fi

log "All libraries built successfully."
