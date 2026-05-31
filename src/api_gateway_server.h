#pragma once
/**
 * @file api_gateway_server.h
 * @brief API-gateway server lifecycle helpers.
 *
 * Design notes
 * ─────────────
 * • CApiGatewaySocket objects are registered in configSingleton.apiGatewaySocketsMap
 *   and live for the process lifetime.
 * • No std::cout — structured logging only.
 * • Forward declaration of startGatewayServer placed before use.
 */

#include "interface/CApiGatewaySocket.h"
#include "config/ConfigSingleton.h"

#include <map>
#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Forward declaration
// ─────────────────────────────────────────────────────────────────────────────

// Defined below — forward-declared so initApiGatewayServers can call it.
inline int startGatewayServer(
        const API_GATEWAY_ENDPOINT &endpoint,
        CApiGatewaySocket *socket,
        PipelineFunction<CApiGatewaySocket> pipelineFunction);

// ─────────────────────────────────────────────────────────────────────────────
// Top-level initialisation (called from main)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Initialise every API-gateway endpoint listed in the configuration.
 *
 * @return 0 on success, the first negative error code on failure.
 */
inline int initApiGatewayServers() {
    const API_GATEWAY_SERVER_CONFIG &gwConfig =
        configSingleton.getApiGatewayConfig();

    for (const auto &endpoint : gwConfig.endpoints) {
        // Register the pipeline so it is available for this endpoint.
        configSingleton.pipelineFactory->registerPipeline<CApiGatewaySocket>(
            endpoint.pipeline);

        auto *sock = new CApiGatewaySocket(endpoint.port);
        configSingleton.apiGatewaySocketsMap[endpoint.name] = sock;

        auto pipelineFn =
            configSingleton.pipelineFactory->GetPipeline<CApiGatewaySocket>(
                endpoint.pipeline);

        int result = startGatewayServer(endpoint, sock, pipelineFn);
        if (result < 0) {
            configSingleton.apiGatewaySocketsMap.erase(endpoint.name);
            delete sock;
            return result;
        }

        LOG_INFO("API-gateway server '" + endpoint.name + "' started on port "
                 + std::to_string(endpoint.port));
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-endpoint start helper
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Configure and start a single API-gateway socket.
 *
 * Builds the host→URI routing map from the endpoint's service list and
 * passes it to the socket before starting it.
 *
 * @return 0 on success, negative on the first failure step.
 */
inline int startGatewayServer(
        const API_GATEWAY_ENDPOINT &endpoint,
        CApiGatewaySocket *socket,
        PipelineFunction<CApiGatewaySocket> pipelineFunction) {
    if (!socket) {
        LOG_ERROR("startGatewayServer: null socket for endpoint '"
                  + endpoint.name + "'");
        return -1;
    }

    if (!socket->SetPipeline(pipelineFunction)) {
        LOG_ERROR("Failed to set pipeline for gateway endpoint '"
                  + endpoint.name + "'");
        return -2;
    }

    // Build host_map: hostname → (path → RESOLVED_SERVICE).
    std::map<std::string, std::map<std::string, RESOLVED_SERVICE>> hostMap;
    for (const auto &service : endpoint.services) {
        std::map<std::string, RESOLVED_SERVICE> uriMap;
        for (const auto &uri : service.uris) {
            if (uri.host.empty()) {
                LOG_ERROR("Gateway endpoint '" + endpoint.name
                          + "': service '" + service.hostname
                          + "' has a URI with an empty host — skipping.");
                continue;
            }
            if (uri.port <= 0 || uri.port > 65535) {
                LOG_ERROR("Gateway endpoint '" + endpoint.name
                          + "': URI port " + std::to_string(uri.port)
                          + " is out of range — skipping.");
                continue;
            }
            RESOLVED_SERVICE resolved{uri.host,
                                      static_cast<unsigned short>(uri.port)};
            uriMap.emplace(uri.path, resolved);
        }
        hostMap[service.hostname] = std::move(uriMap);
    }
    socket->SetApiGatewayConfig(hostMap);

    if (!socket->Start(endpoint.name)) {
        LOG_ERROR("Failed to start API-gateway server '" + endpoint.name + "'");
        return -3;
    }
    return 0;
}
