#include "ConfigParser.h"
#include "ApiGatewayConfigParser.h"
#include "logger/Logger.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers (anonymous namespace = file-private linkage)
// ─────────────────────────────────────────────────────────────────────────────
namespace {

/**
 * @brief Safe text extraction from a tinyxml2 element.
 *
 * Returns an empty string if @p el is null or its text is null, rather than
 * crashing with a null dereference.
 */
inline std::string safeText(const XMLElement *el,
                             const std::string &defaultVal = "") {
    if (!el) return defaultVal;
    const char *txt = el->GetText();
    return txt ? std::string(txt) : defaultVal;
}

/**
 * @brief Convert a string to lower-case in-place.
 */
inline void toLower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// ParseConfiguration
// ─────────────────────────────────────────────────────────────────────────────

XMLError ConfigParser::ParseConfiguration(
        XMLDocument *xmlDoc,
        PROXY_CONFIG &m_ProxyConfig,
        std::vector<PROTOCOL_SERVER_CONFIG> &m_ProtocolConfig,
        API_GATEWAY_SERVER_CONFIG &m_ApiGatewayConfig) {
    if (!xmlDoc) {
        LOG_ERROR("ParseConfiguration: null XMLDocument pointer.");
        return XML_ERROR_FILE_READ_ERROR;
    }

    XMLNode *pRoot = xmlDoc->FirstChildElement("clickhouse-proxy-v2");
    if (!pRoot) {
        LOG_ERROR("ParseConfiguration: root element <clickhouse-proxy-v2> not found.");
        return XML_ERROR_FILE_READ_ERROR;
    }

    LoadProtocolServerConfigurations(pRoot, m_ProtocolConfig);
    LoadProxyServerConfigurations(pRoot, m_ProxyConfig);
    ApiGatewayConfigParser::ParseConfiguration(pRoot, m_ApiGatewayConfig);

    LOG_INFO("Configuration parsed successfully.");
    return XML_SUCCESS;
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadProxyServerConfigurations
// ─────────────────────────────────────────────────────────────────────────────

XMLError ConfigParser::LoadProxyServerConfigurations(XMLNode *pRoot,
                                                      PROXY_CONFIG &m_ProxyConfig) {
    PROXY_CONFIG proxyConfig;

    XMLElement *pClusters = pRoot->FirstChildElement("CLUSTERS");
    if (!pClusters) {
        // No CLUSTERS element — not an error; just means no proxy endpoints.
        m_ProxyConfig = proxyConfig;
        return XML_SUCCESS;
    }

    XMLElement *pCluster = pClusters->FirstChildElement("CLUSTER");
    while (pCluster) {
        CLUSTER cluster;
        const char *clusterName = pCluster->Attribute("name");
        cluster.clusterName = clusterName ? clusterName : "";

        XMLElement *pEndPoints = pCluster->FirstChildElement("END_POINTS");
        if (pEndPoints) {
            XMLElement *pEndPoint = pEndPoints->FirstChildElement("END_POINT");
            while (pEndPoint) {
                REMOTE_END_POINT endPoint;
                const char *epName = pEndPoint->Attribute("name");
                endPoint.endPointName = epName ? epName : "";

                endPoint.readWrite =
                    safeText(pEndPoint->FirstChildElement("READ_WRITE")) == "yes";

                XMLElement *pProxyPort = pEndPoint->FirstChildElement("PROXY_PORT");
                if (pProxyPort) {
                    int port = 0;
                    XMLError err = pProxyPort->QueryIntText(&port);
                    if (err != XML_SUCCESS) {
                        LOG_ERROR("Invalid PROXY_PORT for endpoint '"
                                  + endPoint.endPointName + "'");
                        return err;
                    }
                    if (port <= 0 || port > 65535) {
                        LOG_ERROR("PROXY_PORT " + std::to_string(port)
                                  + " out of range for endpoint '"
                                  + endPoint.endPointName + "'");
                        return XML_ERROR_PARSING_ELEMENT;
                    }
                    endPoint.proxyPort = port;
                }

                endPoint.handler  = safeText(pEndPoint->FirstChildElement("HANDLER"));
                endPoint.pipeline = safeText(pEndPoint->FirstChildElement("PIPELINE"));
                endPoint.loadBalancerStrategy =
                    safeText(pEndPoint->FirstChildElement("loadbalancer-strategy"));

                XMLElement *pServices = pEndPoint->FirstChildElement("SERVICES");
                if (!pServices) {
                    LOG_ERROR("SERVICES element missing for endpoint '"
                              + endPoint.endPointName + "'");
                    return XML_ERROR_PARSING_ELEMENT;
                }

                XMLElement *pService = pServices->FirstChildElement("SERVICE");
                while (pService) {
                    SERVICE service;
                    const char *svcName = pService->Attribute("name");
                    service.name = svcName ? svcName : "";

                    const char *weightAttr = pService->Attribute("weight");
                    if (weightAttr) {
                        try {
                            service.weight = std::stoi(weightAttr);
                        } catch (...) {
                            LOG_ERROR("Invalid weight '" + std::string(weightAttr)
                                      + "' for service '" + service.name + "'");
                        }
                    }

                    service.host            = safeText(pService->FirstChildElement("HOST"));
                    service.source_hostname = safeText(pService->FirstChildElement("SOURCE_HOSTNAME"));

                    XMLElement *pPort = pService->FirstChildElement("PORT");
                    if (pPort) {
                        int port = 0;
                        XMLError err = pPort->QueryIntText(&port);
                        if (err != XML_SUCCESS) {
                            LOG_ERROR("Invalid PORT for service '" + service.name + "'");
                            return err;
                        }
                        service.port = port;
                    }

                    endPoint.services.emplace_back(service);
                    pService = pService->NextSiblingElement("SERVICE");
                }

                cluster.endPoints.emplace_back(endPoint);
                pEndPoint = pEndPoint->NextSiblingElement("END_POINT");
            }
        }

        proxyConfig.clusters.emplace_back(cluster);
        pCluster = pCluster->NextSiblingElement("CLUSTER");
    }

    m_ProxyConfig = proxyConfig;
    return XML_SUCCESS;
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadProtocolServerConfigurations
// ─────────────────────────────────────────────────────────────────────────────

XMLError ConfigParser::LoadProtocolServerConfigurations(
        XMLNode *root,
        std::vector<PROTOCOL_SERVER_CONFIG> &m_ProtocolServerConfig) {
    std::vector<PROTOCOL_SERVER_CONFIG> result;

    XMLElement *psConfig = root->FirstChildElement("protocol-server-config");
    if (!psConfig) {
        // No protocol servers — not an error.
        m_ProtocolServerConfig = result;
        return XML_SUCCESS;
    }

    XMLElement *ps = psConfig->FirstChildElement("protocol-server");
    while (ps) {
        PROTOCOL_SERVER_CONFIG config;
        const char *protocol = ps->Attribute("protocol");
        config.protocol_name = protocol ? protocol : "";

        XMLElement *portEl = ps->FirstChildElement("protocol-port");
        if (portEl) {
            const char *portText = portEl->GetText();
            if (!portText) {
                LOG_ERROR("Empty protocol-port for '" + config.protocol_name + "'");
                return XML_ERROR_PARSING_ELEMENT;
            }
            try {
                config.protocol_port = std::stoi(portText);
            } catch (...) {
                LOG_ERROR("Invalid protocol-port '" + std::string(portText)
                          + "' for '" + config.protocol_name + "'");
                return XML_ERROR_PARSING_ELEMENT;
            }
        }

        config.pipeline = safeText(ps->FirstChildElement("pipeline"));
        config.handler  = safeText(ps->FirstChildElement("handler"));

        XMLElement *authEl = ps->FirstChildElement("auth");
        if (authEl) {
            config.auth           = new AUTH_CONFIG();
            config.auth->strategy = safeText(authEl->FirstChildElement("strategy"));
            config.auth->handler  = safeText(authEl->FirstChildElement("handler"));

            XMLElement *authzEl = authEl->FirstChildElement("authorization");
            if (authzEl) {
                config.auth->authorization           = new AUTHORIZATION_CONFIG();
                config.auth->authorization->strategy =
                    safeText(authzEl->FirstChildElement("strategy"));
                config.auth->authorization->handler =
                    safeText(authzEl->FirstChildElement("handler"));

                XMLElement *dataEl = authzEl->FirstChildElement("data");
                if (dataEl) {
                    XMLPrinter printer;
                    dataEl->Accept(&printer);
                    config.auth->authorization->data = printer.CStr();
                }
            } else {
                config.auth->authorization = nullptr;
            }
        } else {
            config.auth = nullptr;
        }

        XMLElement *routesEl = ps->FirstChildElement("routes");
        if (routesEl) {
            XMLElement *routeEl = routesEl->FirstChildElement("route");
            while (routeEl) {
                Route r;
                r.path            = safeText(routeEl->FirstChildElement("path"));
                r.method          = safeText(routeEl->FirstChildElement("method"));
                r.request_handler = safeText(routeEl->FirstChildElement("request_handler"));
                r.auth.required   = (config.auth != nullptr);

                XMLElement *authCfg = routeEl->FirstChildElement("auth");
                if (authCfg) {
                    XMLElement *reqEl = authCfg->FirstChildElement("required");
                    if (reqEl) {
                        std::string reqStr = safeText(reqEl);
                        toLower(reqStr);
                        if (reqStr == "false" || reqStr == "0" || reqStr == "no") {
                            r.auth.required = false;
                        } else if (reqStr == "true" || reqStr == "1" || reqStr == "yes") {
                            r.auth.required = true;
                        } else {
                            throw std::runtime_error(
                                "Invalid auth.required value '" + reqStr
                                + "' for route: " + r.path);
                        }
                    }

                    XMLElement *authzEl = authCfg->FirstChildElement("authorization");
                    if (authzEl) {
                        XMLPrinter printer;
                        authzEl->Accept(&printer);
                        r.auth.authorization = printer.CStr();
                    }
                }

                config.routes.push_back(r);
                routeEl = routeEl->NextSiblingElement("route");
            }
        }

        result.push_back(config);
        ps = ps->NextSiblingElement("protocol-server");
    }

    m_ProtocolServerConfig = result;
    return XML_SUCCESS;
}

// ─────────────────────────────────────────────────────────────────────────────
// ParseEndPointServiceConfiguration
// ─────────────────────────────────────────────────────────────────────────────

ENDPOINT_SERVICE_CONFIG
ConfigParser::ParseEndPointServiceConfiguration(XMLDocument *xmlDoc) {
    if (!xmlDoc) {
        throw std::runtime_error("ParseEndPointServiceConfiguration: null XMLDocument.");
    }

    ENDPOINT_SERVICE_CONFIG cfg;

    XMLElement *pEndPoint = xmlDoc->FirstChildElement("end_point");
    if (!pEndPoint) {
        throw std::runtime_error("ParseEndPointServiceConfiguration: <end_point> not found.");
    }

    const char *epName = pEndPoint->Attribute("name");
    cfg.name = epName ? epName : "";

    cfg.operation = safeText(pEndPoint->FirstChildElement("operation"));
    if (cfg.operation != "add" && cfg.operation != "delete") {
        throw std::runtime_error(
            "ParseEndPointServiceConfiguration: operation must be 'add' or 'delete', got '"
            + cfg.operation + "'");
    }

    XMLElement *pService = pEndPoint->FirstChildElement("service");
    if (!pService) {
        throw std::runtime_error("ParseEndPointServiceConfiguration: <service> not found.");
    }

    const char *svcName = pService->Attribute("name");
    cfg.service.name = svcName ? svcName : "";
    cfg.service.host = safeText(pService->FirstChildElement("host"));

    XMLElement *pPort = pService->FirstChildElement("port");
    if (pPort) {
        int port = 0;
        XMLError err = pPort->QueryIntText(&port);
        if (err != XML_SUCCESS) {
            throw std::runtime_error("ParseEndPointServiceConfiguration: invalid port value.");
        }
        cfg.service.port = port;
    }

    LOG_INFO("Endpoint service configuration parsed successfully.");
    return cfg;
}
