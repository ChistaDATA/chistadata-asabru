#pragma once
/**
 * @file proxy_server.h
 * @brief Proxy-server lifecycle helpers.
 *
 * Functions in this translation unit own no dynamic memory; every
 * CProxySocket / LibuvProxySocket is registered in ConfigSingleton's maps
 * and lives for the duration of the process.
 *
 * Design notes
 * ─────────────
 * • All helper functions return an int (0 = OK, < 0 = error) so callers can
 *   log a specific failure and bail out early.
 * • No std::cout — structured logging only via LOG_*.
 * • std::string error messages are returned from updateProxyServers() and
 *   updateProxyEndPointService() so callers can forward them over HTTP.
 */

#include "config/ConfigSingleton.h"
#include "interface/CProxySocket.h"
#include "libuv-socket/LibuvServerSocket.h"
#include <algorithm>
#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Apply a resolved configuration to an already-running proxy socket.
 *
 * Replaces the socket's endpoint config and rebuilds its load-balancer server
 * list atomically (from the caller's perspective).
 *
 * @return 0 on success, -1 if @p socket is null.
 */
inline int updateProxyConfig(CProxySocket *socket,
                              const RESOLVED_PROXY_CONFIG &configValue) {
    if (!socket) {
        LOG_ERROR("updateProxyConfig: null socket for endpoint '" + configValue.name + "'");
        return -1;
    }
    TARGET_ENDPOINT_CONFIG cfg{configValue.name, configValue.proxyPort,
                                configValue.services};
    socket->SetConfigValues(cfg);
    socket->loadBalancer->removeAllServers();
    for (const auto &svc : cfg.services) {
        socket->loadBalancer->addServer(svc);
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Hot-reload all proxy endpoint configurations from the singleton.
 *
 * @return Human-readable status message (JSON-friendly — no trailing newline).
 */
inline std::string updateProxyServers() {
    try {
        auto configs = configSingleton.ResolveProxyServerConfigurations();
        for (const auto &cfg : configs) {
            auto it = configSingleton.proxySocketsMap.find(cfg.name);
            if (it == configSingleton.proxySocketsMap.end() || !it->second) {
                LOG_ERROR("updateProxyServers: no socket registered for '"
                          + cfg.name + "' — skipping.");
                continue;
            }
            if (updateProxyConfig(it->second, cfg) < 0) {
                return R"({"status":"error","message":"Failed to update )" + cfg.name + R"("})";
            }
        }
    } catch (const std::exception &e) {
        LOG_ERROR(std::string("updateProxyServers exception: ") + e.what());
        return R"({"status":"error","message":")" + std::string(e.what()) + R"("})";
    }
    return R"({"status":"ok","message":"Configuration updated successfully"})";
}

/**
 * @brief Add or remove a single service from a proxy endpoint.
 *
 * Validates the operation field ("add" | "delete") before modifying state.
 *
 * @return Human-readable status message.
 */
inline std::string updateProxyEndPointService(
        const ENDPOINT_SERVICE_CONFIG &endpointServiceConfig) {
    const std::string &op = endpointServiceConfig.operation;
    if (op != "add" && op != "delete") {
        const std::string msg = "Invalid operation '" + op
                                + "'. Expected 'add' or 'delete'.";
        LOG_ERROR(msg);
        return R"({"status":"error","message":")" + msg + R"("})";
    }

    auto proxyConfig = configSingleton.getProxyConfig();
    bool found = false;

    for (auto &cluster : proxyConfig.clusters) {
        for (auto &endpoint : cluster.endPoints) {
            if (endpoint.endPointName != endpointServiceConfig.name) {
                continue;
            }
            found = true;
            if (op == "add") {
                endpoint.services.push_back(endpointServiceConfig.service);
            } else {
                // op == "delete"
                auto it = std::remove_if(
                    endpoint.services.begin(), endpoint.services.end(),
                    [&endpointServiceConfig](const SERVICE &s) {
                        return s.name == endpointServiceConfig.service.name;
                    });
                endpoint.services.erase(it, endpoint.services.end());
            }
            break;
        }
        if (found) break;
    }

    if (!found) {
        const std::string msg = "Endpoint '" + endpointServiceConfig.name + "' not found.";
        LOG_ERROR(msg);
        return R"({"status":"error","message":")" + msg + R"("})";
    }

    configSingleton.setProxyConfig(proxyConfig);
    return updateProxyServers();
}

// ─────────────────────────────────────────────────────────────────────────────
// Server start helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Configure and start a libuv-based proxy socket.
 *
 * @return 0 on success, negative on the first failure step.
 */
inline int startLibuvProxyServer(
        LibuvProxySocket *socket,
        PipelineFunction<LibuvProxySocket> pipelineFunction,
        const RESOLVED_PROXY_CONFIG &configValue) {
    if (!socket) {
        LOG_ERROR("startLibuvProxyServer: null socket for '" + configValue.name + "'");
        return -1;
    }
    const std::string &name = configValue.name;

    if (!socket->SetPipeline(pipelineFunction)) {
        LOG_ERROR("Failed to set pipeline for '" + name + "'");
        return -2;
    }

    auto *handler = static_cast<CProxyHandler *>(configValue.handler);
    if (!socket->SetHandler(handler)) {
        LOG_ERROR("Failed to set handler for '" + name + "'");
        return -2;
    }

    TARGET_ENDPOINT_CONFIG cfg{configValue.name, configValue.proxyPort,
                                configValue.services};
    if (!socket->SetConfigValues(cfg)) {
        LOG_ERROR("Failed to set config values for '" + name + "'");
        return -2;
    }

    if (!socket->Start(name)) {
        LOG_ERROR("Failed to start libuv proxy server '" + name + "'");
        return -3;
    }
    return 0;
}

/**
 * @brief Configure and start a thread-per-connection proxy socket.
 *
 * @return 0 on success, negative on the first failure step.
 */
inline int startProxyServer(CProxySocket *socket,
                             const RESOLVED_PROXY_CONFIG &configValue) {
    if (!socket) {
        LOG_ERROR("startProxyServer: null socket for '" + configValue.name + "'");
        return -1;
    }
    const std::string &name = configValue.name;

    if (!socket->SetPipeline(configValue.pipeline)) {
        LOG_ERROR("Failed to set pipeline for '" + name + "'");
        return -2;
    }

    auto *handler = static_cast<CProxyHandler *>(configValue.handler);
    if (!socket->SetHandler(handler)) {
        LOG_ERROR("Failed to set handler for '" + name + "'");
        return -2;
    }

    TARGET_ENDPOINT_CONFIG cfg{configValue.name, configValue.proxyPort,
                                configValue.services};
    if (!socket->SetConfigValues(cfg)) {
        LOG_ERROR("Failed to set config values for '" + name + "'");
        return -2;
    }

    if (!socket->Start(name)) {
        LOG_ERROR("Failed to start proxy server '" + name + "'");
        return -3;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level initialisation (called from main)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Initialise every proxy server listed in the configuration.
 *
 * CProxySocket and LibuvProxySocket instances are registered in
 * configSingleton.proxySocketsMap so they outlive this function.
 *
 * @return 0 on success, the first negative error code on failure.
 */
inline int initProxyServers() {
    PipelineFactory pipelineFactory;
    auto configs = configSingleton.ResolveProxyServerConfigurations();

    for (const auto &cfg : configs) {
        int result = 0;

        if (cfg.pipelineName == "ClickHouseLibuvPipeline") {
            auto *sock = new LibuvProxySocket(cfg.proxyPort);
            result = startLibuvProxyServer(
                sock,
                pipelineFactory.GetPipeline<LibuvProxySocket>(cfg.pipelineName),
                cfg);
            if (result < 0) {
                delete sock;
                return result;
            }
            // LibuvProxySocket is not tracked in proxySocketsMap currently;
            // TODO: extend ProxySocketsMap to support both types.
        } else {
            CProxySocket *sock = cfg.loadBalancerStrategy
                ? new CProxySocket(cfg.proxyPort, cfg.loadBalancerStrategy)
                : new CProxySocket(cfg.proxyPort);

            configSingleton.proxySocketsMap[cfg.name] = sock;
            result = startProxyServer(sock, cfg);
            if (result < 0) {
                configSingleton.proxySocketsMap.erase(cfg.name);
                delete sock;
                return result;
            }
        }

        LOG_INFO("Proxy server '" + cfg.name + "' started on port "
                 + std::to_string(cfg.proxyPort));
    }
    return 0;
}
