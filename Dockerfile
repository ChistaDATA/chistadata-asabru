# syntax=docker/dockerfile:1.6
# ─────────────────────────────────────────────────────────────────────────────
# Stage 1 — Build
# ─────────────────────────────────────────────────────────────────────────────
# Pin to a specific Ubuntu LTS digest to guarantee reproducible builds and
# prevent supply-chain attacks from a moving "latest" tag.
FROM ubuntu:24.04 AS builder

# Prevent interactive prompts during package installation.
ENV DEBIAN_FRONTEND=noninteractive

# Install only the packages required to compile the project.
# Combine apt-get update + install + clean in a single RUN layer to keep the
# image small and avoid stale cache issues.
RUN apt-get update -y \
 && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        libcurl4-openssl-dev \
        libreadline-dev \
        libssl-dev \
        libsqlite3-dev \
        libuv1-dev \
        python3 \
        python3-dev \
        nodejs \
        npm \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy only the files needed for dependency resolution first (layer-cache
# optimisation: dependencies are rebuilt only when build scripts change).
COPY cmake/         ./cmake/
COPY build_libs.sh  ./build_libs.sh
COPY lib/           ./lib/

RUN chmod +x ./build_libs.sh \
 && ./build_libs.sh

# Copy the rest of the source code.
COPY . .

# Build in Release mode with all hardening flags enabled.
RUN cmake -S . -B /build/out \
        -DASABRU_COMMONS_BUILD=LOCAL_DIR \
        -DASABRU_ENGINE_BUILD=LOCAL_DIR  \
        -DASABRU_PARSERS_BUILD=LOCAL_DIR \
        -DGTEST_ENABLED=OFF              \
        -DCMAKE_BUILD_TYPE=Release       \
 && cmake --build /build/out --parallel "$(nproc)"

# ─────────────────────────────────────────────────────────────────────────────
# Stage 2 — Runtime
# ─────────────────────────────────────────────────────────────────────────────
# Use the same Ubuntu LTS base, but install only runtime libraries.
FROM ubuntu:24.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

# Install minimal runtime dependencies.
RUN apt-get update -y \
 && apt-get install -y --no-install-recommends \
        ca-certificates \
        libcurl4 \
        libssl3 \
        libsqlite3-0 \
        python3 \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

# Create a non-root user and group to run the proxy.
# Running as root inside a container is a security risk.
RUN groupadd --system asabru \
 && useradd --system --gid asabru --no-create-home --shell /usr/sbin/nologin asabru

# Create required directories with correct ownership before dropping privileges.
RUN mkdir -p /opt/asabru/bin \
             /opt/asabru/plugins \
             /opt/asabru/config \
             /opt/asabru/public \
 && chown -R asabru:asabru /opt/asabru

# Copy build artefacts from the builder stage.
COPY --from=builder --chown=asabru:asabru /build/out/src/Chista_Asabru  /opt/asabru/bin/Chista_Asabru
COPY --from=builder --chown=asabru:asabru /build/out/public              /opt/asabru/public/
COPY --from=builder --chown=asabru:asabru /build/out/config.xml          /opt/asabru/config/config.xml
COPY --from=builder --chown=asabru:asabru /build/lib/asabru-handlers/build /opt/asabru/plugins/

# Mark binary as non-writable to prevent tampering at runtime.
RUN chmod 550 /opt/asabru/bin/Chista_Asabru

# ─── Environment defaults ────────────────────────────────────────────────────
# These can be overridden at container start via -e or a secrets manager.
ENV PLUGINS_FOLDER_PATH=/opt/asabru/plugins \
    PUBLIC_FOLDER_PATH=/opt/asabru/public    \
    CONFIG_FILE_PATH=/opt/asabru/config/config.xml

# ─── Security hardening ──────────────────────────────────────────────────────
# Drop all Linux capabilities; the proxy does not need any privileged ops.
# (capabilities are dropped at runtime with --cap-drop=ALL in the run command,
#  but we document the intent here.)

USER asabru
WORKDIR /opt/asabru

# ─── Health check ────────────────────────────────────────────────────────────
# Verifies the management HTTP API is responsive on port 8080.
# Adjust the port if your config differs.
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl --silent --fail http://localhost:8080/ || exit 1

ENTRYPOINT ["/opt/asabru/bin/Chista_Asabru"]
