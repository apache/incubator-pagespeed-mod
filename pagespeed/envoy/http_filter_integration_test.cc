#include "test/integration/http_integration.h"

namespace Envoy {
class HttpFilterPageSpeedIntegrationTest
    : public HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
 public:
  HttpFilterPageSpeedIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()) {}
  /**
   * Initializer for an individual integration test.
   */
  void SetUp() override { initialize(); }

  void initialize() override {
    config_helper_.addFilter(R"EOF(
name: pagespeed
typed_config:
  "@type": type.googleapis.com/pagespeed.Decoder
  key: "via"
  val: "pagespeed-filter"
)EOF");
    // config_helper_.addFilter("{ name: pagespeed, config: { key: via, val:
    // pagespeed-filter } }");
    HttpIntegrationTest::initialize();
  }
};

INSTANTIATE_TEST_SUITE_P(
    IpVersions, HttpFilterPageSpeedIntegrationTest,
    testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

TEST_P(HttpFilterPageSpeedIntegrationTest, Test1) {
  Envoy::Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/"}, {":authority", "host"}};

  IntegrationCodecClientPtr codec_client;
  FakeHttpConnectionPtr fake_upstream_connection;
  FakeStreamPtr request_stream;

  codec_client = makeHttpConnection(lookupPort("http"));
  auto response = codec_client->makeHeaderOnlyRequest(headers);
  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(
      *dispatcher_, fake_upstream_connection, std::chrono::milliseconds(1000)));
  ASSERT_TRUE(
      fake_upstream_connection->waitForNewStream(*dispatcher_, request_stream));
  ASSERT_TRUE(request_stream->waitForEndStream(*dispatcher_));
  ASSERT_FALSE(response->waitForEndStream());

  // EXPECT_EQ("pagespeed-filter",
  //             request_stream->headers().get(Http::LowerCaseString("via"))->value().getStringView());

  codec_client->close();
}
}  // namespace Envoy
