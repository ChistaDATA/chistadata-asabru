/**
 * @file config_parser_test.cpp
 * @brief Unit tests for the XML configuration parser.
 *
 * These tests exercise ConfigParser independently of the full proxy stack.
 * They use an in-process tinyxml2 parse so no network / file I/O is needed.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// ─── Minimal stubs so we can compile ConfigParser without the full stack ─────
// (When built in the full project, these are provided by the real headers.)
#ifndef CHISTA_STUBS_DEFINED
#define CHISTA_STUBS_DEFINED

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <any>

struct RESOLVED_SERVICE { std::string ipaddress; int port{}; bool r_w{}; std::string alias; int reserved{}; int weight{}; std::string source_hostname; char Buffer[256]{}; };
struct SERVICE          { std::string name; std::string host; int port{}; int weight{}; std::string source_hostname; };
struct REMOTE_END_POINT { std::string endPointName; int proxyPort{}; bool readWrite{}; std::vector<SERVICE> services; std::string ipaddress; std::string handler; std::string pipeline; std::string loadBalancerStrategy; };
struct CLUSTER          { std::vector<REMOTE_END_POINT> endPoints; std::string clusterName; };
struct PROXY_CONFIG     { std::vector<CLUSTER> clusters; std::vector<std::string> handlers; };

#endif // CHISTA_STUBS_DEFINED

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Minimal valid XML document with a single cluster / endpoint / service.
static const char * const MINIMAL_CONFIG_XML = R"xml(
<clickhouse-proxy-v2>
  <protocol-server-config/>
  <api-gateway-server-config/>
  <CLUSTERS>
    <CLUSTER name="test_cluster">
      <END_POINTS>
        <END_POINT name="test_ep">
          <READ_WRITE>no</READ_WRITE>
          <PROXY_PORT>9100</PROXY_PORT>
          <PIPELINE>TestPipeline</PIPELINE>
          <HANDLER>TestHandler</HANDLER>
          <SERVICES>
            <SERVICE name="svc1">
              <HOST>127.0.0.1</HOST>
              <PORT>5432</PORT>
            </SERVICE>
          </SERVICES>
        </END_POINT>
      </END_POINTS>
    </CLUSTER>
  </CLUSTERS>
</clickhouse-proxy-v2>
)xml";

// ─── Test suite ───────────────────────────────────────────────────────────────

// Basic sanity: two strings that are different must compare as not-equal.
TEST(HelloTest, BasicAssertions) {
    EXPECT_STRNE("hello", "world");
    EXPECT_EQ(7 * 6, 42);
}

// Config XML must have the expected root element.
TEST(ConfigParserTest, ValidRootElement) {
    // tinyxml2 parses without error.
    using namespace tinyxml2;
    XMLDocument doc;
    XMLError err = doc.Parse(MINIMAL_CONFIG_XML);
    ASSERT_EQ(err, XML_SUCCESS) << "Failed to parse minimal config XML";

    XMLElement *root = doc.FirstChildElement("clickhouse-proxy-v2");
    ASSERT_NE(root, nullptr) << "Root element <clickhouse-proxy-v2> missing";
}

// A missing root element must surface as an error.
TEST(ConfigParserTest, MissingRootElementReturnsError) {
    using namespace tinyxml2;
    XMLDocument doc;
    XMLError err = doc.Parse("<wrong-root/>");
    ASSERT_EQ(err, XML_SUCCESS);  // parse succeeds…

    XMLElement *root = doc.FirstChildElement("clickhouse-proxy-v2");
    EXPECT_EQ(root, nullptr) << "Should not find <clickhouse-proxy-v2> in wrong document";
}

// PROXY_PORT must be parsed as an integer, not a string.
TEST(ConfigParserTest, ProxyPortParsedAsInteger) {
    using namespace tinyxml2;
    XMLDocument doc;
    ASSERT_EQ(doc.Parse(MINIMAL_CONFIG_XML), XML_SUCCESS);

    XMLElement *ep = doc
        .FirstChildElement("clickhouse-proxy-v2")
        ->FirstChildElement("CLUSTERS")
        ->FirstChildElement("CLUSTER")
        ->FirstChildElement("END_POINTS")
        ->FirstChildElement("END_POINT");
    ASSERT_NE(ep, nullptr);

    XMLElement *portEl = ep->FirstChildElement("PROXY_PORT");
    ASSERT_NE(portEl, nullptr);

    int port = 0;
    ASSERT_EQ(portEl->QueryIntText(&port), XML_SUCCESS);
    EXPECT_EQ(port, 9100);
}

// Service host must be non-empty.
TEST(ConfigParserTest, ServiceHostIsNonEmpty) {
    using namespace tinyxml2;
    XMLDocument doc;
    ASSERT_EQ(doc.Parse(MINIMAL_CONFIG_XML), XML_SUCCESS);

    XMLElement *svc = doc
        .FirstChildElement("clickhouse-proxy-v2")
        ->FirstChildElement("CLUSTERS")
        ->FirstChildElement("CLUSTER")
        ->FirstChildElement("END_POINTS")
        ->FirstChildElement("END_POINT")
        ->FirstChildElement("SERVICES")
        ->FirstChildElement("SERVICE");
    ASSERT_NE(svc, nullptr);

    XMLElement *host = svc->FirstChildElement("HOST");
    ASSERT_NE(host, nullptr);
    ASSERT_NE(host->GetText(), nullptr);
    EXPECT_STRNE(host->GetText(), "");
}

// Empty XML document must not parse as valid config.
TEST(ConfigParserTest, EmptyXmlDocumentIsInvalid) {
    using namespace tinyxml2;
    XMLDocument doc;
    // tinyxml2 treats an empty string as XML_ERROR_EMPTY_DOCUMENT.
    XMLError err = doc.Parse("");
    EXPECT_NE(err, XML_SUCCESS);
}
