# ChistaDATA Asabru

<p align="center">
  <img src="img/logo.svg" alt="ChistaDATA Asabru Logo" width="200"/>
</p>

<p align="center">
  <strong>Enterprise-Class Protocol-Aware Database Proxy</strong><br/>
    High-performance, secure, and scalable database proxy for ClickHouse, PostgreSQL, and MySQL
</p>

<p align="center">
  <a href="https://github.com/ChistaDATA/chista-asabru/actions"><img src="https://github.com/ChistaDATA/chista-asabru/workflows/CI/badge.svg" alt="CI Status"/></a>
    <a href="https://github.com/ChistaDATA/chista-asabru/rleases"><img src="https://img.shields.io/github/v/release/ChistaDATA/chista-asabru" alt="Latest Release"/></a>
      <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache%202.0-blue.svg" alt="License"/></a>
        <a href="docs.asabru.chistadata.io"><img src="https://img.shields.io/badge/docs-official-green.svg" alt="Documentation"/></a>
</p>

---

## Overview

**ChistaDATA Asabru** is an enterprise-grade, protocol-aware database proxy built in C++. It is designed to improve the scalability, availability, and security of database infrastructures running ClickHouse, PostgreSQL, and MySQL. Asabru acts as a transparent intermediary between your applications and database servers, providing connection pooling, load balancing, TLS termination, query routing, and real-time observability — all without requiring changes to your application code.

### Key Value Propositions

- **Zero Application-Side Changes** — Drop Asabru in front of any supported database with no client-side modifications.
- - **Protocol-Native Transparency** — Understands ClickHouse wire protocol, PostgreSQL wire protocol, MySQL wire protocol, HTTP, and HTTPS natively.
  - - **Enterprise Security** — Full TLS/SSL termination and pass-through, mutual TLS (mTLS), and authentication enforcement.
    - - **High Availability** — Built-in load balancing, health checks, and automatic failover across database replicas.
      - - **Observability** — Exposes Prometheus-compatible metrics, Grafana dashboards, and structured query logs out of the box.
        -
        - ---
        -
        - ## Table of Contents
        -
        - - [Architecture](#architecture)
          - - [Features](#features)
            - - [Supported Databases & Protocols](#supported-databases--protocols)
              - - [Quick Start](#quick-start)
                - - [Installation](#installation)
                  -   - [Docker (Recommended)](#docker-recommended)
                      -   - [Build from Source](#build-from-source)
                          - - [Configuration](#configuration)
                            - - [How-To Guides](#how-to-guides)
                              -   - [Guide 1: Proxy ClickHouse with Load Balancing](#guide-1-proxy-clickhouse-with-load-balancing)
                                  -   - [Guide 2: Enable TLS Termination](#guide-2-enable-tls-termination)
                                      -   - [Guide 3: Connect via ChistaDATA DBaaS Portal](#guide-3-connect-via-chistadata-dbaas-portal)
                                          -   - [Guide 4: Monitor with Grafana](#guide-4-monitor-with-grafana)
                                              - - [Examples](#examples)
                                                - - [Testing](#testing)
                                                  - - [Contributing](#contributing)
                                                    - - [License](#license)
                                                      -
                                                      - ---
                                                      -
                                                      - ## Architecture
                                                      -
                                                      - ```
                                                        ┌─────────────────────────────────────────────────────────────┐
                                                        │                     Client Applications                     │
                                                        │          (ClickHouse clients, psql, MySQL clients)          │
                                                        └──────────────────────┬──────────────────────────────────────┘
                                                                               │
                                                                               ▼
                                                        ┌─────────────────────────────────────────────────────────────┐
                                                        │                   ChistaDATA Asabru Proxy                   │
                                                        │  ┌──────────────┐  ┌───────────────┐  ┌──────────────────┐  │
                                                        │  │ Protocol     │  │ Load Balancer │  │ TLS Termination  │  │
                                                        │  │ Parser       │  │ (Round-Robin, │  │ & mTLS Enforcer  │  │
                                                        │  │ (CH/PG/MySQL)│  │  Weighted RR) │  │                  │  │
                                                        │  └──────────────┘  └───────────────┘  └──────────────────┘  │
                                                        │  ┌──────────────┐  ┌───────────────┐  ┌──────────────────┐  │
                                                        │  │ Auth         │  │ Health Check  │  │ Metrics &        │  │
                                                        │  │ Middleware   │  │ & Failover    │  │ Query Logging    │  │
                                                        │  └──────────────┘  └───────────────┘  └──────────────────┘  │
                                                        └──────────────────────┬──────────────────────────────────────┘
                                                                               │
                                                                  ┌────────────┼────────────┐
                                                                  ▼            ▼            ▼
                                                           ┌────────────┐ ┌──────────┐ ┌──────────┐
                                                           │ ClickHouse │ │PostgreSQL│ │  MySQL   │
                                                           │  Cluster   │ │ Cluster  │ │ Cluster  │
                                                           └────────────┘ └──────────┘ └──────────┘
                                                        ```

                                                        ---

                                                        ## Features

                                                        | Feature | ClickHouse | PostgreSQL | MySQL |
                                                        |---|---|---|---|
                                                        | TCP/IP Proxying | ✅ | ✅ | ✅ |
                                                        | HTTP/HTTPS Proxying | ✅ | — | — |
                                                        | TLS/SSL Termination | ✅ | ✅ | 🔄 In Progress |
                                                        | Wire-Level Protocol | ✅ | ✅ | ✅ |
                                                        | Load Balancing | ✅ | ✅ | ✅ |
                                                        | Health Checks | ✅ | ✅ | ✅ |
                                                        | Prometheus Metrics | ✅ | ✅ | ✅ |
                                                        | Query Logging | ✅ | ✅ | ✅ |
                                                        | Connection Pooling | ✅ | ✅ | ✅ |
                                                        | mTLS | ✅ | 🔄 In Progress | 🔄 In Progress |

                                                        ---

                                                        ## Supported Databases & Protocols

                                                        ### ClickHouse
                                                        - **Native TCP Protocol** — port `9000` (default proxy port: `9000`)
                                                        - - **HTTP Protocol** — port `8123` (default proxy port: `8123`)
                                                          - - **HTTPS Protocol** — port `8443` (default proxy port: `8443`)
                                                            - - **TLS-encrypted Native TCP** — port `9440`
                                                              -
                                                              - ### PostgreSQL
                                                              - - **Wire Protocol** — port `5432` (default proxy port: `5432`)
                                                                - - **TLS** — supported via `sslmode=require`
                                                                  -
                                                                  - ### MySQL
                                                                  - - **Wire Protocol** — port `3306` (default proxy port: `3306`)
                                                                    - - TLS support — in progress
                                                                      -
                                                                      - ---
                                                                      -
                                                                      - ## Quick Start
                                                                      -
                                                                      - Get ChistaDATA Asabru running in under 5 minutes using Docker:
                                                                      -
                                                                      - ```bash
                                                                        # 1. Clone the repository
                                                                        git clone https://github.com/ChistaDATA/chista-asabru.git
                                                                        cd chista-asabru

                                                                        # 2. Copy and customize the sample environment file
                                                                        cp .env.sample .env
                                                                        # Edit .env to point to your database host/port

                                                                        # 3. Start the proxy with Docker Compose
                                                                        docker-compose up -d

                                                                        # 4. Verify the proxy is running
                                                                        curl http://localhost:8080/health
                                                                        # Expected: {"status":"ok","version":"1.0.1"}
                                                                        ```

                                                                        The proxy will now accept ClickHouse connections on port `9000` and forward them to the configured backend.

                                                                        ---

                                                                        ## Installation

                                                                        ### Docker (Recommended)

                                                                        **Prerequisites:** Docker 20.10+ and Docker Compose 2.x

                                                                        ```bash
                                                                        # Pull the official image
                                                                        docker pull chistadata/asabru:latest

                                                                        # Run with a configuration file
                                                                        docker run -d \
                                                                          --name asabru \
                                                                          -p 9000:9000 \
                                                                          -p 8123:8123 \
                                                                          -p 8080:8080 \
                                                                          -v $(pwd)/config.xml:/app/config.xml \
                                                                          chistadata/asabru:latest
                                                                        ```

                                                                        ### Build from Source

                                                                        **Prerequisites:** CMake 3.16+, C++17-compatible compiler (GCC 9+ or Clang 10+), OpenSSL 1.1+

                                                                        ```bash
                                                                        # 1. Clone with submodules
                                                                        git clone --recursive https://github.com/ChistaDATA/chista-asabru.git
                                                                        cd chista-asabru

                                                                        # 2. Create build directory
                                                                        mkdir build && cd build

                                                                        # 3. Configure
                                                                        cmake .. -DCMAKE_BUILD_TYPE=Release

                                                                        # 4. Build (replace 4 with your CPU core count)
                                                                        make -j4

                                                                        # 5. Run the proxy
                                                                        ./asabru --config ../config.xml
                                                                        ```

                                                                        ---

                                                                        ## Configuration

                                                                        ChistaDATA Asabru uses an XML-based configuration file. Below is a minimal production-ready example for ClickHouse proxying:

                                                                        ```xml
                                                                        <?xml version="1.0" encoding="UTF-8"?>
                                                                        <configuration>

                                                                          <!-- Proxy identity -->
                                                                          <proxy_name>ChistaDATA Asabru</proxy_name>
                                                                          <log_level>info</log_level>  <!-- debug | info | warn | error -->

                                                                          <!-- ClickHouse TCP listener -->
                                                                          <listeners>
                                                                            <listener>
                                                                              <type>clickhouse_tcp</type>
                                                                              <host>0.0.0.0</host>
                                                                              <port>9000</port>
                                                                              <tls_enabled>false</tls_enabled>
                                                                            </listener>

                                                                            <!-- ClickHouse HTTP listener -->
                                                                            <listener>
                                                                              <type>clickhouse_http</type>
                                                                              <host>0.0.0.0</host>
                                                                              <port>8123</port>
                                                                            </listener>

                                                                            <!-- Admin / Health API -->
                                                                            <listener>
                                                                              <type>admin</type>
                                                                              <host>127.0.0.1</host>
                                                                              <port>8080</port>
                                                                            </listener>
                                                                          </listeners>

                                                                          <!-- Backend ClickHouse servers -->
                                                                          <backends>
                                                                            <backend>
                                                                              <host>clickhouse-primary.internal</host>
                                                                              <port>9000</port>
                                                                              <weight>3</weight>
                                                                              <max_connections>200</max_connections>
                                                                            </backend>
                                                                            <backend>
                                                                              <host>clickhouse-replica1.internal</host>
                                                                              <port>9000</port>
                                                                              <weight>1</weight>
                                                                              <max_connections>100</max_connections>
                                                                            </backend>
                                                                          </backends>

                                                                          <!-- Load balancing strategy: round_robin | weighted_round_robin | least_connections -->
                                                                          <load_balancer>weighted_round_robin</load_balancer>

                                                                          <!-- Health checks -->
                                                                          <health_check>
                                                                            <enabled>true</enabled>
                                                                            <interval_seconds>10</interval_seconds>
                                                                            <timeout_seconds>3</timeout_seconds>
                                                                            <unhealthy_threshold>2</unhealthy_threshold>
                                                                            <healthy_threshold>1</healthy_threshold>
                                                                          </health_check>

                                                                          <!-- Metrics -->
                                                                          <metrics>
                                                                            <prometheus_enabled>true</prometheus_enabled>
                                                                            <prometheus_port>9090</prometheus_port>
                                                                          </metrics>

                                                                        </configuration>
                                                                        ```

                                                                        See [docs/configuration.md](docs/configuration.md) for the full configuration reference.

                                                                        ---

                                                                        ## How-To Guides

                                                                        ### Guide 1: Proxy ClickHouse with Load Balancing

                                                                        This guide shows how to configure ChistaDATA Asabru to distribute queries across a ClickHouse cluster using weighted round-robin.

                                                                        **Step 1: Define your backends in `config.xml`**

                                                                        ```xml
                                                                        <backends>
                                                                          <backend>
                                                                            <host>ch-node1.example.com</host>
                                                                            <port>9000</port>
                                                                            <weight>2</weight>
                                                                          </backend>
                                                                          <backend>
                                                                            <host>ch-node2.example.com</host>
                                                                            <port>9000</port>
                                                                            <weight>1</weight>
                                                                          </backend>
                                                                        </backends>
                                                                        <load_balancer>weighted_round_robin</load_balancer>
                                                                        ```

                                                                        **Step 2: Start the proxy**

                                                                        ```bash
                                                                        ./asabru --config config.xml
                                                                        ```

                                                                        **Step 3: Connect your ClickHouse client to the proxy**

                                                                        ```bash
                                                                        # Using clickhouse-client
                                                                        clickhouse-client --host localhost --port 9000

                                                                        # Using HTTP
                                                                        curl "http://localhost:8123/?query=SELECT+1"
                                                                        ```

                                                                        **Step 4: Verify load balancing via metrics**

                                                                        ```bash
                                                                        curl http://localhost:9090/metrics | grep asabru_backend_connections
                                                                        ```

                                                                        ---

                                                                        ### Guide 2: Enable TLS Termination

                                                                        Secure client connections with TLS while keeping backend connections unencrypted on your private network.

                                                                        **Step 1: Generate or obtain your TLS certificate**

                                                                        ```bash
                                                                        # Self-signed for development (use a CA-signed cert in production)
                                                                        openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt \
                                                                          -days 365 -nodes -subj '/CN=asabru.example.com'
                                                                        ```

                                                                        **Step 2: Configure TLS in `config.xml`**

                                                                        ```xml
                                                                        <listeners>
                                                                          <listener>
                                                                            <type>clickhouse_tcp</type>
                                                                            <host>0.0.0.0</host>
                                                                            <port>9440</port>
                                                                            <tls_enabled>true</tls_enabled>
                                                                            <tls_cert>/etc/asabru/certs/server.crt</tls_cert>
                                                                            <tls_key>/etc/asabru/certs/server.key</tls_key>
                                                                          </listener>
                                                                        </listeners>
                                                                        ```

                                                                        **Step 3: Connect using TLS**

                                                                        ```bash
                                                                        clickhouse-client --host localhost --port 9440 --secure
                                                                        ```

                                                                        ---

                                                                        ### Guide 3: Connect via ChistaDATA DBaaS Portal

                                                                        ChistaDATA Asabru is natively integrated with the ChistaDATA DBaaS Portal.

                                                                        1. Log in to the [ChistaDATA DBaaS Portal](https://dbaas.chistadata.io).
                                                                        2. 2. Create or select a ClickHouse cluster.
                                                                           3. 3. On the **Cluster Creation** screen, enable **"Deploy Proxy"**.
                                                                              4. 4. The proxy will be automatically deployed and configured for your cluster.
                                                                                 5. 5. The proxy endpoint will be available at your cluster's proxy host on port `9000` (ClickHouse native) or `8123` (HTTP).
                                                                                    6.
                                                                                    7. No additional configuration is required. The portal manages certificate rotation, health checks, and scaling automatically.
                                                                                    8.
                                                                                    9. ---
                                                                                    10.
                                                                                    11. ### Guide 4: Monitor with Grafana
                                                                                    12.
                                                                                    13. ChistaDATA Asabru ships with a pre-built Grafana dashboard.
                                                                                    14.
                                                                                    15. **Step 1: Start Prometheus + Grafana**
                                                                                    16.
                                                                                    17. ```yaml
                                                                                        # docker-compose.monitoring.yml
                                                                                        version: '3.8'
                                                                                        services:
                                                                                          prometheus:
                                                                                            image: prom/prometheus:latest
                                                                                            volumes:
                                                                                              - ./prometheus.yml:/etc/prometheus/prometheus.yml
                                                                                            ports:
                                                                                              - "9091:9090"

                                                                                          grafana:
                                                                                            image: grafana/grafana:latest
                                                                                            ports:
                                                                                              - "3000:3000"
                                                                                            environment:
                                                                                              - GF_SECURITY_ADMIN_PASSWORD=admin
                                                                                        ```

                                                                                        **Step 2: Configure Prometheus scrape target**

                                                                                        ```yaml
                                                                                        # prometheus.yml
                                                                                        scrape_configs:
                                                                                          - job_name: 'asabru'
                                                                                            static_configs:
                                                                                              - targets: ['asabru:9090']
                                                                                        ```

                                                                                        **Step 3: Import the Asabru dashboard**

                                                                                        In Grafana, go to **Dashboards → Import** and upload the dashboard from [automation_testing/grafana-dashboard.json](automation_testing/).

                                                                                        See [docs/grafana-datasources.md](docs/grafana-datasources.md) for details.

                                                                                        ---

                                                                                        ## Examples

                                                                                        ### Example 1: Basic ClickHouse HTTP Query via Proxy

                                                                                        ```bash
                                                                                        # Query ClickHouse through the HTTP proxy
                                                                                        curl -s "http://localhost:8123/?query=SELECT+version()"
                                                                                        # Output: 23.8.1.2992

                                                                                        curl -s "http://localhost:8123/?query=SELECT+count()+FROM+system.tables"
                                                                                        # Output: 142
                                                                                        ```

                                                                                        ### Example 2: Python Application Using the Proxy

                                                                                        ```python
                                                                                        import clickhouse_connect

                                                                                        # Connect to ChistaDATA Asabru proxy instead of directly to ClickHouse
                                                                                        client = clickhouse_connect.get_client(
                                                                                            host='asabru.internal',
                                                                                            port=8123,
                                                                                            username='default',
                                                                                            password='',
                                                                                            database='analytics'
                                                                                        )

                                                                                        result = client.query('SELECT count() FROM events WHERE date >= today() - 7')
                                                                                        print(f"Events in last 7 days: {result.first_row[0]}")
                                                                                        ```

                                                                                        ### Example 3: Go Application Using the Proxy

                                                                                        ```go
                                                                                        package main

                                                                                        import (
                                                                                            "database/sql"
                                                                                            "fmt"
                                                                                            _ "github.com/ClickHouse/clickhouse-go/v2"
                                                                                        )

                                                                                        func main() {
                                                                                            // Point your DSN to ChistaDATA Asabru
                                                                                            dsn := "clickhouse://default:@asabru.internal:9000/analytics"
                                                                                            db, err := sql.Open("clickhouse", dsn)
                                                                                            if err != nil {
                                                                                                panic(err)
                                                                                            }
                                                                                            defer db.Close()

                                                                                            var count uint64
                                                                                            row := db.QueryRow("SELECT count() FROM events")
                                                                                            row.Scan(&count)
                                                                                            fmt.Printf("Total events: %d\n", count)
                                                                                        }
                                                                                        ```

                                                                                        ### Example 4: PostgreSQL Application Using the Proxy

                                                                                        ```python
                                                                                        import psycopg2

                                                                                        # Connect to PostgreSQL through ChistaDATA Asabru
                                                                                        conn = psycopg2.connect(
                                                                                            host="asabru.internal",
                                                                                            port=5432,
                                                                                            dbname="mydb",
                                                                                            user="postgres",
                                                                                            password="secret"
                                                                                        )

                                                                                        cursor = conn.cursor()
                                                                                        cursor.execute("SELECT version();")
                                                                                        print(cursor.fetchone())
                                                                                        conn.close()
                                                                                        ```

                                                                                        ### Example 5: Health Check & Admin API

                                                                                        ```bash
                                                                                        # Check proxy health
                                                                                        curl http://localhost:8080/health
                                                                                        # {"status":"ok","version":"1.0.1","backends":{"healthy":2,"total":2}}

                                                                                        # View backend status
                                                                                        curl http://localhost:8080/backends
                                                                                        # {"backends":[{"host":"ch-node1.example.com","port":9000,"status":"healthy","connections":12},{"host":"ch-node2.example.com","port":9000,"status":"healthy","connections":6}]}

                                                                                        # View current configuration (read-only)
                                                                                        curl http://localhost:8080/config
                                                                                        ```

                                                                                        ---

                                                                                        ## Testing

                                                                                        ChistaDATA Asabru has a comprehensive test suite covering unit tests, integration tests, and end-to-end automation tests.

                                                                                        ### Running Unit Tests

                                                                                        ```bash
                                                                                        cd build
                                                                                        ctest --output-on-failure
                                                                                        ```

                                                                                        This runs all tests registered via CMake, including:

                                                                                        - **`hello_test — Baseline GoogleTest framework validation
                                                                                        - - **`config_parser_test`** — Full XML configuration parser unit tests (valid configs, missing fields, malformed XML, edge cases)
                                                                                          -
                                                                                          - ### Running Integration Tests
                                                                                          -
                                                                                          - Integration tests require a running ClickHouse instance. Set the target host via environment variable:
                                                                                          -
                                                                                          - ```bash
                                                                                          - export ASABRU_TEST_CH_HOST=localhost
                                                                                          - export ASABRU_TEST_CH_PORT=9000
                                                                                          - cd build && ctest -R integration --output-on-failure
                                                                                          - ```

                                                                                            ### Running Automation Tests

                                                                                            End-to-end automation tests are located in the [`automation_testing/`](automation_testing/) directory and cover proxy behavior, load balancing, failover, and metrics collection.

                                                                                            ```bash
                                                                                            cd automation_testing
                                                                                            pip install -r requirements.txt
                                                                                            pytest -v
                                                                                            ```

                                                                                            ### Test Matrix

                                                                                            | Test Suite | Coverage | Framework |
                                                                                            |---|---|---|
                                                                                            | Config Parser | XML parsing, validation, edge cases | GoogleTest |
                                                                                            | Protocol Handler | ClickHouse TCP, HTTP, PostgreSQL wire | GoogleTest |
                                                                                            | Load Balancer | Round-robin, weighted, least-conn | GoogleTest |
                                                                                            | TLS Termination | Certificate loading, handshake | GoogleTest |
                                                                                            | End-to-End (CH) | Full query flow via proxy | pytest |
                                                                                            | End-to-End (PG) | Full query flow via proxy | pytest |
                                                                                            | Grafana Metrics | Prometheus scrape + dashboard | pytest |

                                                                                            ### Test Case Examples

                                                                                            **Config Parser Test (`test/config_parser_test.cpp`)**

                                                                                            ```cpp
                                                                                            #include <gtest/gtest.h>
                                                                                            #include "ConfigParser.h"

                                                                                            // Test: Valid configuration loads correctly
                                                                                            TEST(ConfigParserTest, ValidConfigLoadsSuccessfully) {
                                                                                                ConfigParser parser;
                                                                                                ASSERT_TRUE(parser.load("test/fixtures/valid_config.xml"));
                                                                                                EXPECT_EQ(parser.getProxyName(), "ChistaDATA Asabru");
                                                                                                EXPECT_EQ(parser.getBackends().size(), 2);
                                                                                                EXPECT_EQ(parser.getLoadBalancerStrategy(), "weighted_round_robin");
                                                                                            }

                                                                                            // Test: Missing required field raises error
                                                                                            TEST(ConfigParserTest, MissingBackendHostRaisesError) {
                                                                                                ConfigParser parser;
                                                                                                EXPECT_THROW(parser.load("test/fixtures/missing_host_config.xml"),
                                                                                                             std::invalid_argument);
                                                                                            }

                                                                                            // Test: Default values applied when optional fields absent
                                                                                            TEST(ConfigParserTest, DefaultValuesAppliedForOptionalFields) {
                                                                                                ConfigParser parser;
                                                                                                parser.load("test/fixtures/minimal_config.xml");
                                                                                                EXPECT_EQ(parser.getLogLevel(), "info");
                                                                                                EXPECT_EQ(parser.getHealthCheckIntervalSeconds(), 10);
                                                                                            }
                                                                                            ```

                                                                                            ---

                                                                                            ## Deployment

                                                                                            ### Production Checklist

                                                                                            - [ ] Replace self-signed certificates with CA-signed certificates
                                                                                            - [ ] - [ ] Set `log_level` to `warn` or `error` in production
                                                                                            - [ ] - [ ] Configure health check thresholds appropriate for your SLA
                                                                                            - [ ] - [ ] Enable Prometheus metrics and connect Grafana dashboard
                                                                                            - [ ] - [ ] Set `max_connections` per backend based on your database capacity
                                                                                            - [ ] - [ ] Review and apply network firewall rules (only expose necessary ports)
                                                                                            - [ ] - [ ] Use Docker secrets or a secrets manager for TLS private keys
                                                                                            - [ ] - [ ] Enable log rotation for query logs
                                                                                            - [ ]
                                                                                            - [ ] ### Kubernetes Deployment
                                                                                            - [ ]
                                                                                            - [ ] A Helm chart is available at [ChistaDATA/helm-charts](https://github.com/ChistaDATA/helm-charts):
                                                                                            - [ ]
                                                                                            - [ ] ```bash
                                                                                            - [ ] helm repo add chistadata https://charts.chistadata.io
                                                                                            - [ ] helm install asabru chistadata/asabru \
                                                                                            - [ ]   --set backends[0].host=clickhouse.default.svc.cluster.local \
                                                                                            - [ ]     --set backends[0].port=9000
                                                                                            - [ ] ```
                                                                                            - [ ]
                                                                                            - [ ] ---
                                                                                            - [ ]
                                                                                            - [ ] ## Contributing
                                                                                            - [ ]
                                                                                            - [ ] We welcome contributions from the community! Please read our [Developer Documentation](DEVELOPER.md) before submitting a pull request.
                                                                                            - [ ]
                                                                                            - [ ] ### Development Setup
                                                                                            - [ ]
                                                                                            - [ ] ```bash
                                                                                            - [ ] git clone --recursive https://github.com/ChistaDATA/chista-asabru.git
                                                                                            - [ ] cd chista-asabru
                                                                                            - [ ] mkdir build && cd build
                                                                                            - [ ] cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON
                                                                                            - [ ] make -j4
                                                                                            - [ ] ctest
                                                                                            - [ ] ```
                                                                                            - [ ]
                                                                                            - [ ] ### Contribution Guidelines
                                                                                            - [ ]
                                                                                            - [ ] - Follow the coding style enforced by `.clang-format`
                                                                                            - [ ] - Add unit tests for all new features
                                                                                            - [ ] - Update documentation in `docs/` alongside code changes
                                                                                            - [ ] - Open an issue before starting work on large features
                                                                                            - [ ]
                                                                                            - [ ] ---
                                                                                            - [ ]
                                                                                            - [ ] ## Documentation
                                                                                            - [ ]
                                                                                            - [ ] | Resource | Link |
                                                                                            - [ ] |---|---|
                                                                                            - [ ] | Official Documentation | [docs.asabru.chistadata.io](https://docs.asabru.chistadata.io) |
                                                                                            - [ ] | Configuration Reference | [docs/configuration.md](docs/configuration.md) |
                                                                                            - [ ] | API Reference | [docs/apis.md](docs/apis.md) |
                                                                                            - [ ] | Load Balancing Strategies | [docs/loadbalancer-strategies.md](docs/loadbalancer-strategies.md) |
                                                                                            - [ ] | Authentication | [docs/authentication-api.md](docs/authentication-api.md) |
                                                                                            - [ ] | Grafana Setup | [docs/grafana-datasources.md](docs/grafana-datasources.md) |
                                                                                            - [ ] | Cluster Monitor | [docs/cluster-monitor-client.md](docs/cluster-monitor-client.md) |
                                                                                            - [ ] | Developer Guide | [DEVELOPER.md](DEVELOPER.md) |
                                                                                            - [ ]
                                                                                            - [ ] ---
                                                                                            - [ ]
                                                                                            - [ ] ## License
                                                                                            - [ ]
                                                                                            - [ ] ChistaDATA Asabru is released under the [Apache License 2.0](LICENSE).
                                                                                            - [ ]
                                                                                            - [ ] ---
                                                                                            - [ ]
                                                                                            - [ ] ## About ChistaDATA
                                                                                            - [ ]
                                                                                            - [ ] [ChistaDATA Inc.](https://www.chistadata.io) specializes in enterprise ClickHouse solutions, managed database services, and open-source tooling for analytical databases. ChistaDATA Asabru is the company's flagship open-source proxy, powering production workloads at scale.
                                                                                            - [ ]
                                                                                            - [ ] - **Website:** [chistadata.io](https://www.chistadata.io)
                                                                                            - [ ] - **DBaaS Portal:** [dbaas.chistadata.io](https://dbaas.chistadata.io)
                                                                                            - [ ] - **Documentation:** [docs.asabru.chistadata.io](https://docs.asabru.chistadata.io)
                                                                                            - [ ] - **Twitter / X:** [@ChistaDATA](https://twitter.com/ChistaDATA)
                                                                                            - [ ] - **LinkedIn:** [ChistaDATA](https://www.linkedin.com/company/chistadata)`**</strong>
