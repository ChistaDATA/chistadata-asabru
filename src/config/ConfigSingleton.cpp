#include "ConfigSingleton.h"
#include "CommandDispatcher.h"
#include "Utils.h"
#include "logger/Logger.h"

#include <curl/curl.h>   // libcurl — replaces the unsafe system("curl …") call
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

// Initialise static member.
bool ConfigSingleton::initialized = false;

// ─────────────────────────────────────────────────────────────────────────────
// libcurl write-callback helper (internal linkage)
// ─────────────────────────────────────────────────────────────────────────────
namespace {
size_t curlWriteCallback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *stream = static_cast<std::ofstream *>(userdata);
    const auto bytes = size * nmemb;
    stream->write(static_cast<const char *>(ptr), static_cast<std::streamsize>(bytes));
    return stream->good() ? bytes : 0;
}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// DownloadConfigFile
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Download the configuration file from @p url to @p outputFilePath.
 *
 * Uses libcurl directly — **never** calls system() or popen() with
 * user-supplied data.  TLS peer verification is enabled.
 *
 * @throws std::runtime_error if the download fails.
 */
void ConfigSingleton::DownloadConfigFile(const std::string &url,
                                          const std::string &outputFilePath) {
    if (url.empty() || outputFilePath.empty()) {
        throw std::runtime_error(
            "DownloadConfigFile: URL and output path must not be empty.");
    }

    // Reject non-HTTP(S) schemes to prevent SSRF via file:// / ftp:// etc.
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        throw std::runtime_error(
            "DownloadConfigFile: only http:// and https:// URLs are accepted.");
    }

    std::ofstream outFile(outputFilePath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        throw std::runtime_error(
            "DownloadConfigFile: cannot open output file: " + outputFilePath);
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("DownloadConfigFile: curl_easy_init() failed.");
    }

    // --- cURL options ---
    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &outFile);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS,      3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);    // 30-second timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);     // enforce TLS verification
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "chista-asabru/1.1");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    outFile.close();

    if (res != CURLE_OK) {
        throw std::runtime_error(
            std::string("DownloadConfigFile: curl error: ")
            + curl_easy_strerror(res));
    }

    LOG_INFO("Config file downloaded successfully to: " + outputFilePath);
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadConfigurationsFromFile
// ─────────────────────────────────────────────────────────────────────────────

XMLError ConfigSingleton::LoadConfigurationsFromFile(std::string filePath) {
    if (filePath.empty()) {
        LOG_ERROR("LoadConfigurationsFromFile: empty file path.");
        return XML_ERROR_FILE_NOT_FOUND;
    }

    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(filePath.c_str());
    if (eResult != XML_SUCCESS) {
        LOG_ERROR("LoadConfigurationsFromFile: failed to load '" + filePath
                  + "' (error " + std::to_string(eResult) + ")");
        return eResult;
    }

    return ConfigParser::ParseConfiguration(
        &xmlDoc, m_ProxyConfig, m_ProtocolServerConfig, m_ApiGatewayServerConfig);
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadConfigurationsFromString
// ─────────────────────────────────────────────────────────────────────────────

XMLError ConfigSingleton::LoadConfigurationsFromString(std::string xml_string) {
    if (xml_string.empty()) {
        LOG_ERROR("LoadConfigurationsFromString: empty XML string.");
        return XML_ERROR_EMPTY_DOCUMENT;
    }

    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.Parse(xml_string.c_str(),
                                     xml_string.size());
    if (eResult != XML_SUCCESS) {
        LOG_ERROR("LoadConfigurationsFromString: parse error "
                  + std::to_string(eResult));
        return eResult;
    }

    return ConfigParser::ParseConfiguration(
        &xmlDoc, m_ProxyConfig, m_ProtocolServerConfig, m_ApiGatewayServerConfig);
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadEndpointServiceFromString
// ─────────────────────────────────────────────────────────────────────────────

ENDPOINT_SERVICE_CONFIG
ConfigSingleton::LoadEndpointServiceFromString(const std::string &xml_string) {
    if (xml_string.empty()) {
        throw std::runtime_error(
            "LoadEndpointServiceFromString: empty XML string.");
    }

    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.Parse(xml_string.c_str(),
                                     xml_string.size());
    if (eResult != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error(
            "LoadEndpointServiceFromString: XML parse error "
            + std::to_string(eResult));
    }

    return ConfigParser::ParseEndPointServiceConfiguration(&xmlDoc);
}

// ─────────────────────────────────────────────────────────────────────────────
// ResolveProxyServerConfigurations
// ─────────────────────────────────────────────────────────────────────────────

std::vector<RESOLVED_PROXY_CONFIG>
ConfigSingleton::ResolveProxyServerConfigurations() const {
    std::vector<RESOLVED_PROXY_CONFIG> results;

    for (const auto &cluster : m_ProxyConfig.clusters) {
        for (const auto &endpoint : cluster.endPoints) {
            RESOLVED_PROXY_CONFIG result;
            result.name      = endpoint.endPointName;
            result.proxyPort = endpoint.proxyPort;

            if (result.proxyPort <= 0 || result.proxyPort > 65535) {
                throw std::runtime_error(
                    "Invalid proxy port " + std::to_string(result.proxyPort)
                    + " for endpoint '" + result.name + "'");
            }

            for (const auto &service : endpoint.services) {
                RESOLVED_SERVICE rs;
                rs.ipaddress       = service.host;
                rs.port            = service.port;
                rs.r_w             = endpoint.readWrite;
                rs.alias           = "";
                rs.reserved        = 0;
                rs.weight          = service.weight;
                rs.source_hostname = service.source_hostname;
                std::memset(rs.Buffer, 0, sizeof rs.Buffer);
                result.services.push_back(rs);
            }

            // Load balancer strategy
            result.loadBalancerStrategy =
                endpoint.loadBalancerStrategy.empty()
                ? nullptr
                : loadBalancerFactory->GetLoadBalancerStrategy(
                      endpoint.loadBalancerStrategy);

            // Pipeline
            result.pipelineName = endpoint.pipeline;
            pipelineFactory->registerPipeline<CProxySocket>(endpoint.pipeline);
            result.pipeline =
                pipelineFactory->GetPipeline<CProxySocket>(endpoint.pipeline);

            // Handler
            CommandDispatcher::RegisterCommand<BaseHandler>(endpoint.handler);
            result.handler =
                CommandDispatcher::GetCommand<BaseHandler>(endpoint.handler);

            results.push_back(result);
        }
    }

    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
// ResolveProtocolServerConfigurations
// ─────────────────────────────────────────────────────────────────────────────

std::vector<RESOLVED_PROTOCOL_CONFIG>
ConfigSingleton::ResolveProtocolServerConfigurations() {
    std::vector<RESOLVED_PROTOCOL_CONFIG> results;

    for (const PROTOCOL_SERVER_CONFIG &proto : m_ProtocolServerConfig) {
        RESOLVED_PROTOCOL_CONFIG result;
        result.protocol_name = proto.protocol_name;
        result.protocol_port = proto.protocol_port;

        if (result.protocol_port <= 0 || result.protocol_port > 65535) {
            throw std::runtime_error(
                "Invalid protocol port "
                + std::to_string(result.protocol_port)
                + " for protocol '" + result.protocol_name + "'");
        }

        // Pipeline
        pipelineFactory->registerPipeline<CProtocolSocket>(proto.pipeline);
        result.pipeline =
            pipelineFactory->GetPipeline<CProtocolSocket>(proto.pipeline);
        if (!result.pipeline) {
            throw std::runtime_error(
                "Pipeline '" + proto.pipeline + "' not found for protocol '"
                + result.protocol_name + "'");
        }

        // Handler
        CommandDispatcher::RegisterCommand<BaseHandler>(proto.handler);
        result.handler =
            CommandDispatcher::GetCommand<BaseHandler>(proto.handler);

        // Authentication / authorisation
        if (proto.auth) {
            result.auth          = new RESOLVED_PROTOCOL_AUTH_CONFIG();
            result.auth->strategy =
                authenticationFactory->createAuthenticationStrategy(
                    proto.auth->strategy);
            result.auth->handler = proto.auth->handler;

            if (proto.auth->authorization) {
                result.auth->authorization =
                    new RESOLVED_PROTOCOL_AUTHORIZATION_CONFIG();
                ComputationContext ctx;
                ctx.Put(AUTHORIZATION_TYPE_KEY,
                        proto.auth->authorization->strategy);
                ctx.Put(AUTHORIZATION_DATA_KEY,
                        proto.auth->authorization->data);
                result.auth->authorization->strategy =
                    authorizationFactory->createAuthorizationStrategy(&ctx);
                result.auth->authorization->handler =
                    proto.auth->authorization->handler;
            } else {
                result.auth->authorization = nullptr;
            }
        } else {
            result.auth = nullptr;
        }

        result.routes = proto.routes;
        results.push_back(result);
    }

    return results;
}
