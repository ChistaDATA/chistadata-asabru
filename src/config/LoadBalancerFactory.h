#pragma once
/**
 * @file LoadBalancerFactory.h
 * @brief Factory that maps strategy names to load-balancer instances.
 *
 * Ownership
 * ─────────
 * The factory owns the strategy objects.  Strategies are allocated on
 * construction and freed in the destructor.  Each strategy is stateless (or
 * holds only thread-safe state), so one instance is shared across all proxy
 * sockets that use the same strategy name.
 */

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "config/config_types.h"
#include "load_balancer/RoundRobinStrategy.h"
#include "load_balancer/RandomStrategy.h"
#include "load_balancer/WeightedRoundRobinStrategy.h"

typedef std::map<std::string, LoadBalancerStrategy<RESOLVED_SERVICE> *>
    LoadBalancerStrategyMap;

class LoadBalancerFactory {
private:
    LoadBalancerStrategyMap m_strategyMap;

public:
    LoadBalancerFactory() {
        // Use raw owning pointers — consistent with the rest of the codebase.
        // Freed in the destructor.
        m_strategyMap["RoundRobinStrategy"]         = new RoundRobinStrategy<RESOLVED_SERVICE>();
        m_strategyMap["RandomStrategy"]             = new RandomStrategy<RESOLVED_SERVICE>();
        m_strategyMap["WeightedRoundRobinStrategy"] = new WeightedRoundRobinStrategy<RESOLVED_SERVICE>();
    }

    ~LoadBalancerFactory() {
        for (auto &pair : m_strategyMap) {
            delete pair.second;
            pair.second = nullptr;
        }
    }

    // Non-copyable: raw pointer ownership makes copy semantics unsafe.
    LoadBalancerFactory(const LoadBalancerFactory &)            = delete;
    LoadBalancerFactory &operator=(const LoadBalancerFactory &) = delete;

    /**
     * @brief Look up a load-balancer strategy by name.
     *
     * @param strategyName  Name as used in the XML config (e.g. "RoundRobinStrategy").
     * @return Non-owning pointer to the strategy, or nullptr if not found.
     *
     * @note Returns nullptr (not a sentinel) for unknown names so callers
     *       can fall back to a default strategy without throwing.
     */
    LoadBalancerStrategy<RESOLVED_SERVICE> *GetLoadBalancerStrategy(
            const std::string &strategyName) {
        auto it = m_strategyMap.find(strategyName);
        if (it == m_strategyMap.end()) {
            // Unknown strategy — log and return nullptr so callers can decide.
            LOG_ERROR("LoadBalancerFactory: unknown strategy '" + strategyName
                      + "'. Falling back to no load-balancer.");
            return nullptr;
        }
        return it->second;
    }
};
