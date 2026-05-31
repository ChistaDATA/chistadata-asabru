#pragma once
/**
 * @file PipelineFactory.h
 * @brief Dynamic pipeline loader via dlopen/dlsym.
 *
 * Security notes
 * ──────────────
 * • The plugins folder path is set at startup from an environment variable.
 *   Only files with the correct extension (.so / .dylib) are loaded.
 * • dlopen errors throw std::runtime_error instead of calling exit() so the
 *   caller can handle the failure gracefully.
 * • RTLD_NOW is used instead of RTLD_LAZY to surface symbol resolution errors
 *   at load time rather than at first call.
 *
 * Thread-safety
 * ─────────────
 * updateLibs() and registerPipeline() must be called before any proxy threads
 * start.  GetPipeline() is read-only and safe to call from multiple threads
 * after initialisation.
 */

#include <dlfcn.h>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "TypeFactory.h"
#include "interface/CApiGatewaySocket.h"
#include "interface/CProtocolSocket.h"
#include "interface/CProxySocket.h"
#include "interface/LibuvProxySocket.h"
#include "socket/Socket.h"
#include "logger/Logger.h"

typedef std::map<std::string, PipelineFunction<CProtocolSocket>>   ProtocolPipelineFunctionMap;
typedef std::map<std::string, PipelineFunction<CProxySocket>>      ProxyPipelineFunctionMap;
typedef std::map<std::string, PipelineFunction<LibuvProxySocket>>  LibuvProxyPipelineFunctionMap;
typedef std::map<std::string, PipelineFunction<CApiGatewaySocket>> ApiGatewayPipelineFunctionMap;

class PipelineFactory {
private:
    ProtocolPipelineFunctionMap    m_protocolMap;
    ProxyPipelineFunctionMap       m_proxyMap;
    LibuvProxyPipelineFunctionMap  m_libuvMap;
    ApiGatewayPipelineFunctionMap  m_apiGwMap;
    std::set<std::string>          m_libNames;

public:
    PipelineFactory() { updateLibs(); }

    // Non-copyable: dlopen handles are not safely copyable.
    PipelineFactory(const PipelineFactory &)            = delete;
    PipelineFactory &operator=(const PipelineFactory &) = delete;

    // ── Public interface ───────────────────────────────────────────────────

    /**
     * @brief Scan the plugins folder and register any new .so / .dylib files.
     *
     * @throws std::runtime_error if PLUGINS_FOLDER_PATH is not set.
     */
    void updateLibs();

    /**
     * @brief Return the pipeline function for @p pipelineName.
     *
     * @return Function pointer, or nullptr if not registered.
     */
    template <typename T>
    PipelineFunction<T> GetPipeline(const std::string &pipelineName) {
        if constexpr (std::is_same_v<T, CProxySocket>) {
            auto it = m_proxyMap.find(pipelineName);
            return (it != m_proxyMap.end()) ? it->second : nullptr;
        } else if constexpr (std::is_same_v<T, CApiGatewaySocket>) {
            auto it = m_apiGwMap.find(pipelineName);
            return (it != m_apiGwMap.end()) ? it->second : nullptr;
        } else if constexpr (std::is_same_v<T, CProtocolSocket>) {
            auto it = m_protocolMap.find(pipelineName);
            return (it != m_protocolMap.end()) ? it->second : nullptr;
        } else if constexpr (std::is_same_v<T, LibuvProxySocket>) {
            auto it = m_libuvMap.find(pipelineName);
            return (it != m_libuvMap.end()) ? it->second : nullptr;
        }
        return nullptr;
    }

    /**
     * @brief Load and cache the pipeline function for @p pipelineName.
     *
     * Idempotent: if the pipeline is already loaded, this is a no-op.
     *
     * @throws std::runtime_error on dlopen / dlsym failure.
     */
    template <class T>
    void registerPipeline(const std::string &pipelineName) {
        if constexpr (std::is_same_v<T, CProxySocket>) {
            loadPipeline<PipelineFunction<CProxySocket>>(pipelineName, m_proxyMap);
        } else if constexpr (std::is_same_v<T, CApiGatewaySocket>) {
            loadPipeline<PipelineFunction<CApiGatewaySocket>>(pipelineName, m_apiGwMap);
        } else if constexpr (std::is_same_v<T, CProtocolSocket>) {
            loadPipeline<PipelineFunction<CProtocolSocket>>(pipelineName, m_protocolMap);
        } else if constexpr (std::is_same_v<T, LibuvProxySocket>) {
            loadPipeline<PipelineFunction<LibuvProxySocket>>(pipelineName, m_libuvMap);
        }
    }

private:
    /**
     * @brief dlopen the shared library that exports @p pluginName and cache
     *        the symbol in @p functionMap.
     *
     * Library naming convention:
     *   plugin name X  →  file lib<path>/libX.<ext>
     *   The helper libnameothy() strips the path, "lib" prefix, and extension.
     *
     * @throws std::runtime_error on dlopen / dlsym failure or if the plugin
     *         library is not found in the scanned plugins folder.
     */
    template <class T>
    void loadPipeline(const std::string &pluginName,
                      std::map<std::string, T> &functionMap) {
        // Find the library file that corresponds to this plugin name.
        auto it = std::find_if(
            m_libNames.begin(), m_libNames.end(),
            [&pluginName](const std::string &libPath) {
                return pluginName == libnameothy(libPath).first;
            });

        if (it == m_libNames.end()) {
            throw std::runtime_error(
                "Pipeline plugin not found in plugins folder: " + pluginName);
        }

        const std::string &libPath = *it;
        auto [baseName, symbolName] = libnameothy(libPath);

        // Idempotent: do not re-load if we already have this symbol.
        if (functionMap.count(symbolName) && functionMap[symbolName] != nullptr) {
            return;
        }

        // Use RTLD_NOW to catch missing symbols at load time.
        void *handle = dlopen(libPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            throw std::runtime_error(
                "dlopen('" + libPath + "') failed: " + std::string(dlerror()));
        }

        // Clear previous dlerror before calling dlsym.
        dlerror();
        T pipelineFunc = reinterpret_cast<T>(dlsym(handle, symbolName.c_str()));
        const char *err = dlerror();
        if (err) {
            dlclose(handle);
            throw std::runtime_error(
                "dlsym('" + symbolName + "') failed in '" + libPath
                + "': " + std::string(err));
        }

        functionMap[symbolName] = pipelineFunc;
        LOG_INFO("Loaded pipeline '" + symbolName + "' from '" + libPath + "'");
    }
};
