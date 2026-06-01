# syntax=docker/dockerfile:1.6
# -------------------------------------------------------------------------
# Stage 1 -- Build
# -------------------------------------------------------------------------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install all build-time dependencies.
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

COPY cmake/ ./cmake/
COPY build_libs.sh ./build_libs.sh
COPY lib/ ./lib/

RUN chmod +x ./build_libs.sh && ./build_libs.sh

COPY . .

RUN cmake -S . -B /build/out \
        -DASABRU_COMMONS_BUILD=LOCAL_DIR \
        -DASABRU_ENGINE_BUILD=LOCAL_DIR \
        -DASABRU_PARSERS_BUILD=LOCAL_DIR \
        -DGTEST_ENABLED=OFF \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build/out --parallel "$(nproc)"

# -------------------------------------------------------------------------
# Stage 2 -- Runtime
# -------------------------------------------------------------------------
FROM ubuntu:24.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

# Install minimal runtime dependencies.
# IMPORTANT: curl (the CLI tool) is required for the HEALTHCHECK below.
RUN apt-get update -y \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        libcurl4 \
        libssl3 \
        libsqlite3-0 \
        python3 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user.
RUN groupadd --system asabru \
    && useradd --system --gid asabru --no-create-home --shell /usr/sbin/nologin asabru

RUN mkdir -p /opt/asabru/bin \
             /opt/asabru/plugins \
             /opt/asabru/config \
             /opt/asabru/public \
    && chown -R asabru:asabru /opt/asabru

COPY --from=builder --chown=asabru:asabru /build/out/src/Chista_Asabru /opt/asabru/bin/Chista_Asabru
COPY --from=builder --chown=asabru:asabru /build/out/config.xml /opt/asabru/config/config.xml
COPY --from=builder --chown=asabru:asabru /build/lib/asabru-handlers/build /opt/asabru/plugins/

# Copy UI public assets only if they were built (submodule may be absent).
RUN --mount=type=bind,from=builder,source=/build/out,target=/bld \
    sh -c '[ -d /bld/public ] && cp -r /bld/public/. /opt/asabru/public/ \
           && chown -R asabru:asabru /opt/asabru/public || true'

RUN chmod 550 /opt/asabru/bin/Chista_Asabru

ENV PLUGINS_FOLDER_PATH=/opt/asabru/plugins \
    PUBLIC_FOLDER_PATH=/opt/asabru/public \
    CONFIG_FILE_PATH=/opt/asabru/config/config.xml

USER asabru
WORKDIR /opt/asabru

# curl CLI is installed above; health check will succeed.
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl --silent --fail http://localhost:8080/ || exit 1

ENTRYPOINT ["/opt/asabru/bin/Chista_Asabru"]
