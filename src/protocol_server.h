#pragma once
/**
 * @file protocol_server.h
 * @brief Protocol-server (HTTP management API) lifecycle helpers.
 *
 * This translation unit wires together the HTTP-based management API:
 *  • /auth/login, /auth/logout — session authentication
 *  • /configuration           — hot-reload the proxy XML config
 *  • /updateService           — add / remove a backend service
 *  • /python, /upload, /…     — as registered in the XML config
 *
 * Design notes
 * ─────────────
 * • Functions return int (0 = OK, < 0 = error) — consistent with proxy_server.h.
 * • CProtocolSocket objects are registered in configSingleton.protocolSocketsMap
 *   and live for the process lifetime.
 * • No std::cout — structured logging only.
 */

#include "BaseComputationCommand.h"
#include "CommandDispatcher.h"
#include "proxy_server.h"
#include "protocol-handlers/CHttpProtocolHandler.h"
#include "interface/CProtocolSocket.h"
#include "config/ConfigSingleton.h"

#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Callback helpers (invoked from HTTP route handlers)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Reload the full proxy configuration from an XML string.
 *
 * @return JSON-formatted status message suitable for an HTTP response body.
 */
inline std::string updateConfiguration(std::string content) {
    try {
        configSingleton.LoadConfigurationsFromString(std::move(content));
        return updateProxyServers();
    } catch (const std::exception &e) {
        LOG_ERROR(std::string("updateConfiguration exception: ") + e.what());
        return R"({"status":"error","message":")" + std::string(e.what()) + R"("})";
    }
}

/**
 * @brief Add or remove a single backend service from an endpoint.
 *
 * @return JSON-formatted status message.
 */
inline std::string updateEndPointService(std::string content) {
    try {
        ENDPOINT_SERVICE_CONFIG cfg =
            configSingleton.LoadEndpointServiceFromString(std::move(content));
        return updateProxyEndPointService(cfg);
    } catch (const std::exception &e) {
        LOG_ERROR(std::string("updateEndPointService exception: ") + e.what());
        return R"({"status":"error","message":")" + std::string(e.what()) + R"("})";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Server start helper
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Configure and start a single CProtocolSocket.
 *
 * @return 0 on success, negative on the first failure step.
 */
inline int startProtocolServer(CProtocolSocket *socket,
                                const RESOLVED_PROTOCOL_CONFIG &configValue) {
    if (!socket) {
        LOG_ERROR("startProtocolServer: null socket for protocol '"
                  + configValue.protocol_name + "'");
        return -1;
    }
    const std::string &name = configValue.protocol_name;

    if (!socket->SetPipeline(configValue.pipeline)) {
        LOG_ERROR("Failed to set pipeline for protocol server '" + name + "'");
        return -2;
    }

    auto *handler = static_cast<CProtocolHandler *>(configValue.handler);
    if (handler && !socket->SetHandler(handler)) {
        LOG_ERROR("Failed to set handler for protocol server '" + name + "'");
        return -2;
    }

    if (!socket->Start()) {
        LOG_ERROR("Failed to start protocol server '" + name + "' on port "
                  + std::to_string(configValue.protocol_port));
        return -3;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level initialisation (called from main)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Initialise every protocol server listed in the configuration.
 *
 * Each server's socket is registered in configSingleton.protocolSocketsMap.
 *
 * @return 0 on success, the first negative error code on failure.
 */
inline int initProtocolServers() {
    auto configs = configSingleton.ResolveProtocolServerConfigurations();

    for (const auto &cfg : configs) {
        auto *httpHandler = static_cast<CHttpProtocolHandler *>(cfg.handler);

        if (httpHandler) {
            for (const auto &route : cfg.routes) {
                // Validate that the method is known before registering.
                auto methodIt = HttpMethodEnumMap.find(route.method);
                if (methodIt == HttpMethodEnumMap.end()) {
                    LOG_ERROR("Unknown HTTP method '" + route.method
                              + "' for route '" + route.path + "' — skipping.");
                    continue;
                }

                httpHandler->RegisterHttpRequestHandler(
                    route.path,
                    methodIt->second,
                    // Capture by value so the lambda remains valid after cfg
                    // goes out of scope.
                    [route, cfg](const simple_http_server::HttpRequest &request)
                        -> simple_http_server::HttpResponse {

                        LOG_INFO("Handling " + route.method + " " + route.path);

                        ComputationContext context;
                        context.Put("request",              &request);
                        context.Put("update_configuration", static_cast<std::function<std::string(std::string)>>(updateConfiguration));
                        context.Put("update_endpoint_service", static_cast<std::function<std::string(std::string)>>(updateEndPointService));
                        context.Put(AUTHENTICATION_STRATEGY_KEY, cfg.auth ? cfg.auth->strategy : nullptr);

                        // ── Authentication ──────────────────────────────
                        if (route.auth.required && cfg.auth) {
                            CommandDispatcher::Dispatch(cfg.auth->handler, &context);
                            const auto authenticated =
                                context.Get(AUTH_AUTHENTICATED_KEY);
                            bool isAuth = false;
                            try { isAuth = std::any_cast<bool>(authenticated); }
                            catch (...) {}

                            if (!isAuth) {
                                LOG_INFO("Authentication failed for " + route.path);
                                simple_http_server::HttpResponse resp(
                                    simple_http_server::HttpStatusCode::Unauthorized);
                                resp.SetHeader("Content-Type", "application/json");
                                resp.SetContent(R"({"status":"error","message":"Unauthorized"})");
                                return resp;
                            }

                            // ── Authorization ───────────────────────────
                            if (!route.auth.authorization.empty()
                                    && cfg.auth->authorization) {
                                context.Put(AUTHORIZATION_DATA_KEY,
                                            route.auth.authorization);
                                context.Put(AUTHORIZATION_STRATEGY_KEY,
                                            cfg.auth->authorization->strategy);
                                CommandDispatcher::Dispatch(
                                    cfg.auth->authorization->handler, &context);

                                bool isAuthorized = false;
                                try {
                                    isAuthorized = std::any_cast<bool>(
                                        context.Get(AUTHORIZATION_AUTHORIZED_KEY));
                                } catch (...) {}

                                if (!isAuthorized) {
                                    LOG_INFO("Authorization failed for " + route.path);
                                    simple_http_server::HttpResponse resp(
                                        simple_http_server::HttpStatusCode::Forbidden);
                                    resp.SetHeader("Content-Type", "application/json");
                                    resp.SetContent(R"({"status":"error","message":"Forbidden"})");
                                    return resp;
                                }
                            }
                        }

                        // ── Dispatch to request handler ─────────────────
                        try {
                            CommandDispatcher::Dispatch(route.request_handler,
                                                        &context);
                            return std::any_cast<simple_http_server::HttpResponse>(
                                context.Get("response"));
                        } catch (const std::exception &e) {
                            LOG_ERROR("Request handler '" + route.request_handler
                                      + "' threw: " + e.what());
                            simple_http_server::HttpResponse resp(
                                simple_http_server::HttpStatusCode::InternalServerError);
                            resp.SetHeader("Content-Type", "application/json");
                            resp.SetContent(R"({"status":"error","message":"Internal server error"})");
                            return resp;
                        }
                    }
                );
            }
        }

        // Allocate socket; ownership transfers to protocolSocketsMap.
        auto *sock = new CProtocolSocket(cfg.protocol_port);
        configSingleton.protocolSocketsMap[cfg.protocol_name] = sock;

        int result = startProtocolServer(sock, cfg);
        if (result < 0) {
            configSingleton.protocolSocketsMap.erase(cfg.protocol_name);
            delete sock;
            return result;
        }

        LOG_INFO("Protocol server '" + cfg.protocol_name + "' started on port "
                 + std::to_string(cfg.protocol_port));
    }
    return 0;
}
