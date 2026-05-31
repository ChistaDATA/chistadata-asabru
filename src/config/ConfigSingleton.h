#pragma once
/**
 * @file ConfigSingleton.h
 * @brief Global configuration singleton for the Chista Asabru proxy.
 *
 * This singleton owns all configuration state and the factory objects used to
 * create pipelines, load-balancers, authentication, and authorisation
 * strategies.  It is intentionally not copyable or movable.
 *
 * Thread-safety
 * ─────────────
 * The singleton is initialised exactly once (at static-initialisation time via
 * getInstance()).  After that, reads of the configuration maps are safe from
 * multiple threads.  Hot-reload (setProxyConfig / LoadConfigurationsFromString)
 * must be called from a single writer thread (enforced by the HTTP management
 * API which serialises writes through its request queue).
 */

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

// Third-party / internal includes
#include "TypeFactory.h"
#include "tinyxml2.h"
#include "CommonTypes.h"
#include "ConfigTypes.h"
#include "interface/CProxyHandler.h"
#include "PipelineFactory.h"
#include "ConfigParser.h"
#include "LoadBalancerFactory.h"
#include "authentication/AuthenticationFactory.h"
#include "authorization/AuthorizationFactory.h"

using namespace tinyxml2;

// ─────────────────────────────────────────────────────────────────────────────
// Socket-map typedefs
// ─────────────────────────────────────────────────────────────────────────────
typedef std::map<std::string, CProxySocket *>      ProxySocketsMap;
typedef std::map<std::string, CProtocolSocket *>   ProtocolSocketsMap;
typedef std::map<std::string, CApiGatewaySocket *> ApiGatewaySocketsMap;

// ─────────────────────────────────────────────────────────────────────────────
// ConfigSingleton
// ─────────────────────────────────────────────────────────────────────────────
class ConfigSingleton {
private:
    // ── Construction ────────────────────────────────────────────────────────
    ConfigSingleton() {
        const char *configFilePath = std::getenv("CONFIG_FILE_PATH");
        if (!configFilePath || configFilePath[0] == '\0') {
            throw std::runtime_error(
                "Environment variable CONFIG_FILE_PATH is not set or empty.");
        }

        const char *configFileUrl = std::getenv("CONFIG_FILE_URL");
        if (configFileUrl && configFileUrl[0] != '\0') {
            // Download the configuration file via libcurl (never system()).
            DownloadConfigFile(std::string(configFileUrl),
                               std::string(configFilePath));
        }

        XMLError err = LoadConfigurationsFromFile(std::string(configFilePath));
        if (err != XML_SUCCESS) {
            throw std::runtime_error(
                "Failed to load configuration from '" +
                std::string(configFilePath) + "' (tinyxml2 error " +
                std::to_string(err) + ")");
        }

        initialized = true;
    }

    ~ConfigSingleton() = default;

    // Non-copyable, non-movable.
    ConfigSingleton(const ConfigSingleton &)            = delete;
    ConfigSingleton &operator=(const ConfigSingleton &) = delete;
    ConfigSingleton(ConfigSingleton &&)                 = delete;
    ConfigSingleton &operator=(ConfigSingleton &&)      = delete;

    // ── Configuration state ──────────────────────────────────────────────────
    PROXY_CONFIG                      m_ProxyConfig;
    std::vector<PROTOCOL_SERVER_CONFIG> m_ProtocolServerConfig;
    API_GATEWAY_SERVER_CONFIG         m_ApiGatewayServerConfig;

    // Guards hot-reload writes.
    mutable std::mutex m_configMutex;

    // ── Private helpers ──────────────────────────────────────────────────────
    XMLError LoadConfigurationsFromFile(std::string filePath);

public:
    // ── Singleton accessor ───────────────────────────────────────────────────
    static bool initialized;

    static ConfigSingleton &getInstance() {
        static ConfigSingleton instance;
        return instance;
    }

    // ── Configuration accessors ──────────────────────────────────────────────
    PROXY_CONFIG getProxyConfig() const {
        std::lock_guard<std::mutex> lock(m_configMutex);
        return m_ProxyConfig;
    }

    void setProxyConfig(PROXY_CONFIG proxyConfig) {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_ProxyConfig = std::move(proxyConfig);
    }

    API_GATEWAY_SERVER_CONFIG getApiGatewayConfig() const {
        std::lock_guard<std::mutex> lock(m_configMutex);
        return m_ApiGatewayServerConfig;
    }

    // ── Dynamic (re-)load ────────────────────────────────────────────────────
    XMLError LoadConfigurationsFromString(std::string xml_string);
    static ENDPOINT_SERVICE_CONFIG LoadEndpointServiceFromString(
        const std::string &xml_string);

    // ── Resolution (builds runtime structures from config) ───────────────────
    std::vector<RESOLVED_PROXY_CONFIG> ResolveProxyServerConfigurations() const;
    std::vector<RESOLVED_PROTOCOL_CONFIG> ResolveProtocolServerConfigurations();

    // ── File download (uses libcurl — never system()) ────────────────────────
    void DownloadConfigFile(const std::string &url,
                            const std::string &outputFilePath);

    // ── Factories (owned by singleton) ───────────────────────────────────────
    PipelineFactory      *pipelineFactory      = new PipelineFactory();
    LoadBalancerFactory  *loadBalancerFactory  = new LoadBalancerFactory();
    AuthenticationFactory *authenticationFactory = new AuthenticationFactory();
    AuthorizationFactory  *authorizationFactory  = new AuthorizationFactory();

    // ── Live socket maps ──────────────────────────────────────────────────────
    ProxySocketsMap      proxySocketsMap;
    ProtocolSocketsMap   protocolSocketsMap;
    ApiGatewaySocketsMap apiGatewaySocketsMap;
};

// ─────────────────────────────────────────────────────────────────────────────
// Process-wide singleton reference
// ─────────────────────────────────────────────────────────────────────────────
// Declared in a header with static linkage to avoid ODR violations when
// multiple translation units include this header.
static ConfigSingleton &configSingleton = ConfigSingleton::getInstance();
