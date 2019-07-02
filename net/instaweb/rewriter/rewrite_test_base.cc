/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include "net/instaweb/rewriter/public/rewrite_test_base.h"

#include <cstddef>
#include <new>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_cache_failure.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/log_record_test_helper.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/css_url_encoder.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/base64_util.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/delay_cache.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/cache/mock_time_cache.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/html/html_writer_filter.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"
#include "pagespeed/kernel/util/gzip_inflater.h"
#include "pagespeed/kernel/util/simple_stats.h"
#include "pagespeed/kernel/util/url_multipart_encoder.h"
#include "pagespeed/opt/logging/log_record.h"

namespace net_instaweb {

namespace {

const char kPsaWasGzipped[] = "x-psa-was-gzipped";

// Logging at the INFO level slows down tests, adds to the noise, and
// adds considerably to the speed variability.
class RewriteTestBaseProcessContext : public ProcessContext {
 public:
  RewriteTestBaseProcessContext() {
    logging::SetMinLogLevel(logging::LOG_WARNING);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RewriteTestBaseProcessContext);
};
RewriteTestBaseProcessContext rewrite_test_base_process_context;

class TestRewriteOptionsManager : public RewriteOptionsManager {
 public:
  TestRewriteOptionsManager()
      : options_(NULL) {}

  void GetRewriteOptions(const GoogleUrl& url,
                         const RequestHeaders& headers,
                         OptionsCallback* done) {
    done->Run((options_ == NULL) ? NULL : options_->Clone());
  }

  void set_options(RewriteOptions* options) { options_ = options; }

 private:
  RewriteOptions* options_;
};

}  // namespace

const char RewriteTestBase::kTestData[] = "/net/instaweb/rewriter/testdata/";
const char RewriteTestBase::kConfiguredBeaconingKey[] =
    "configured_beaconing_key";
const char RewriteTestBase::kWrongBeaconingKey[] = "wrong_beaconing_key";
const char kMessagePatternShrinkImage[] = "*Shrinking image*";

RewriteTestBase::RewriteTestBase()
    : kFoundResult(HTTPCache::kFound, kFetchStatusOK),
      kNotFoundResult(HTTPCache::kNotFound, kFetchStatusNotSet),
      factory_(new TestRewriteDriverFactory(rewrite_test_base_process_context,
                                            GTestTempDir(),
                                            &mock_url_fetcher_)),
      other_factory_(new TestRewriteDriverFactory(
          rewrite_test_base_process_context,
          GTestTempDir(),
          &mock_url_fetcher_)),
      use_managed_rewrite_drivers_(false),
      options_(factory_->NewRewriteOptions()),
      other_options_(other_factory_->NewRewriteOptions()),
      kEtag0(HTTPCache::FormatEtag("0")),
      expected_nonce_(0) {
  statistics_.reset(new SimpleStats(factory_->thread_system()));
  Init();
}

RewriteTestBase::RewriteTestBase(
    std::pair<TestRewriteDriverFactory*, TestRewriteDriverFactory*> factories)
    : kFoundResult(HTTPCache::kFound, kFetchStatusOK),
      kNotFoundResult(HTTPCache::kNotFound, kFetchStatusNotSet),
      factory_(factories.first),
      other_factory_(factories.second),
      use_managed_rewrite_drivers_(false),
      options_(factory_->NewRewriteOptions()),
      other_options_(other_factory_->NewRewriteOptions()),
      expected_nonce_(0) {
  statistics_.reset(new SimpleStats(factory_->thread_system()));
  Init();
}

void RewriteTestBase::Init() {
  DCHECK(statistics_ != NULL);
  RewriteDriverFactory::Initialize();
  TestRewriteDriverFactory::InitStats(statistics_.get());
  factory_->SetStatistics(statistics_.get());
  other_factory_->SetStatistics(statistics_.get());
  server_context_ = factory_->CreateServerContext();
  other_server_context_ = other_factory_->CreateServerContext();
  active_server_ = kPrimary;
  message_handler_.set_mutex(factory_->thread_system()->NewMutex());
}

RewriteTestBase::~RewriteTestBase() {
  RewriteDriverFactory::Terminate();
}

// The Setup/Constructor split is designed so that test subclasses can
// add options prior to calling RewriteTestBase::SetUp().
void RewriteTestBase::SetUp() {
  HtmlParseTestBaseNoAlloc::SetUp();
  http_cache()->SetCompressionLevel(options_->http_cache_compression_level());
  rewrite_driver_ = MakeDriver(server_context_, options_);
  other_server_context()->http_cache()->SetCompressionLevel(
      options_->http_cache_compression_level());
  other_rewrite_driver_ = MakeDriver(other_server_context_, other_options_);
}

void RewriteTestBase::TearDown() {
  if (use_managed_rewrite_drivers_) {
    factory_->ShutDown();
    other_factory_->ShutDown();
  } else {
    rewrite_driver_->WaitForShutDown();

    // We need to make sure we shutdown the threads here before
    // deleting the driver, as the last task on the rewriter's job
    // queue may still be wrapping up some cleanups and notifications.
    factory_->ShutDown();
    rewrite_driver_->Clear();
    delete rewrite_driver_;
    rewrite_driver_ = NULL;

    other_rewrite_driver_->WaitForShutDown();
    other_factory_->ShutDown();
    other_rewrite_driver_->Clear();
    delete other_rewrite_driver_;
    other_rewrite_driver_ = NULL;
  }
  HtmlParseTestBaseNoAlloc::TearDown();
}

// Adds rewrite filters related to recompress images.
void RewriteTestBase::AddRecompressImageFilters() {
  // TODO(vchudnov): Consider adding kConvertToWebpLossless.
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRecompressWebp);
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
}

// Add a single rewrite filter to rewrite_driver_.
void RewriteTestBase::AddFilter(RewriteOptions::Filter filter) {
  options()->EnableFilter(filter);
  rewrite_driver_->AddFilters();
}

// Add a single rewrite filter to other_rewrite_driver_.
void RewriteTestBase::AddOtherFilter(RewriteOptions::Filter filter) {
  other_options()->EnableFilter(filter);
  other_rewrite_driver_->AddFilters();
}

void RewriteTestBase::AddRewriteFilter(RewriteFilter* filter) {
  rewrite_driver_->RegisterRewriteFilter(filter);
  rewrite_driver_->EnableRewriteFilter(filter->id());
}

void RewriteTestBase::AddFetchOnlyRewriteFilter(RewriteFilter* filter) {
  rewrite_driver_->RegisterRewriteFilter(filter);
}

void RewriteTestBase::AddOtherRewriteFilter(RewriteFilter* filter) {
  other_rewrite_driver_->RegisterRewriteFilter(filter);
  other_rewrite_driver_->EnableRewriteFilter(filter->id());
}

void RewriteTestBase::SetBaseUrlForFetch(const StringPiece& url) {
  rewrite_driver_->SetBaseUrlForFetch(url);
}

void RewriteTestBase::ParseUrl(StringPiece url, StringPiece html_input) {
  if (rewrite_driver_->request_headers() == NULL) {
    SetDriverRequestHeaders();
  }
  HtmlParseTestBaseNoAlloc::ParseUrl(url, html_input);
}

void RewriteTestBase::PopulateRequestHeaders(RequestHeaders* request_headers) {
  request_headers->Add(HttpAttributes::kUserAgent, current_user_agent_);
  request_headers->Add(HttpAttributes::kAcceptEncoding, HttpAttributes::kGzip);
  CHECK_EQ(request_attribute_names_.size(), request_attribute_values_.size());
  for (size_t i = 0, n = request_attribute_names_.size(); i < n; ++i) {
    request_headers->Add(request_attribute_names_[i],
                         request_attribute_values_[i]);
  }
}

void RewriteTestBase::SetDriverRequestHeaders() {
  RequestHeaders request_headers;
  PopulateRequestHeaders(&request_headers);
  rewrite_driver()->SetRequestHeaders(request_headers);
}

void RewriteTestBase::AddRequestAttribute(StringPiece name, StringPiece value) {
  request_attribute_names_.push_back(name.as_string());
  request_attribute_values_.push_back(value.as_string());
}

void RewriteTestBase::SetDownstreamCacheDirectives(
    StringPiece downstream_cache_purge_method,
    StringPiece downstream_cache_purge_location_prefix,
    StringPiece rebeaconing_key) {
  options_->ClearSignatureForTesting();
  options_->set_downstream_cache_rewritten_percentage_threshold(95);
  options_->set_downstream_cache_purge_method(downstream_cache_purge_method);
  options_->set_downstream_cache_purge_location_prefix(
      downstream_cache_purge_location_prefix);
  options_->set_downstream_cache_rebeaconing_key(rebeaconing_key);
  options_->ComputeSignature();
}

void RewriteTestBase::SetShouldBeaconHeader(StringPiece rebeaconing_key) {
  AddRequestAttribute(kPsaShouldBeacon, rebeaconing_key);
  SetDriverRequestHeaders();
}

ResourcePtr RewriteTestBase::CreateResource(const StringPiece& base,
                                            const StringPiece& url) {
  rewrite_driver_->SetBaseUrlForFetch(base);
  GoogleUrl base_url(base);
  GoogleUrl resource_url(base_url, url);
  bool unused;
  return rewrite_driver_->CreateInputResource(
      resource_url, RewriteDriver::InputRole::kUnknown, &unused);
}

void RewriteTestBase::PopulateDefaultHeaders(
    const ContentType& content_type, int64 original_content_length,
    ResponseHeaders* headers) {
  int64 time = timer()->NowUs();
  // Reset mock timer so synthetic headers match original.  This temporarily
  // fakes out the mock_scheduler, but we will repair the damage below.
  AdjustTimeUsWithoutWakingAlarms(start_time_ms() * Timer::kMsUs);
  SetDefaultLongCacheHeaders(&content_type, headers);
  // Then set it back.  Note that no alarms should fire at this point
  // because alarms work on absolute time.
  AdjustTimeUsWithoutWakingAlarms(time);
  if (original_content_length > 0) {
    headers->SetOriginalContentLength(original_content_length);
  }
}

void RewriteTestBase::AppendDefaultHeaders(
    const ContentType& content_type, GoogleString* text) {
  ResponseHeaders headers;
  PopulateDefaultHeaders(content_type, 0, &headers);
  StringWriter writer(text);
  headers.WriteAsHttp(&writer, message_handler());
}

void RewriteTestBase::AppendDefaultHeadersWithCanonical(
    const ContentType& content_type, StringPiece canon, GoogleString* text) {
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kLink,
              StrCat("<", canon, ">; rel=\"canonical\""));
  PopulateDefaultHeaders(content_type, 0, &headers);
  StringWriter writer(text);

  // Find how long the origin is to populate x-original-content-length.
  RequestContextPtr request_context(CreateRequestContext());
  StringAsyncFetch fetch(request_context);
  mock_url_fetcher_.Fetch(canon.as_string(), message_handler(), &fetch);
  int64 length;
  ASSERT_TRUE(fetch.done());
  ASSERT_TRUE(fetch.success());
  if (!fetch.response_headers()->FindContentLength(&length)) {
    length = fetch.buffer().size();
  }
  headers.SetOriginalContentLength(length);

  headers.WriteAsHttp(&writer, message_handler());
}

void RewriteTestBase::ServeResourceFromManyContexts(
    const GoogleString& resource_url,
    const StringPiece& expected_content) {
  ServeResourceFromNewContext(resource_url, expected_content);
}

void RewriteTestBase::ServeResourceFromManyContextsWithUA(
    const GoogleString& resource_url,
    const StringPiece& expected_content,
    const StringPiece& user_agent) {
  // TODO(sligocki): Serve the resource under several contexts. For example:
  //   1) With output-resource cached,
  //   2) With output-resource not cached, but in a file,
  //   3) With output-resource unavailable, but input-resource cached,
  //   4) With output-resource unavailable and input-resource not cached,
  //      but still fetchable,
  SetCurrentUserAgent(user_agent);
  ServeResourceFromNewContext(resource_url, expected_content);
  //   5) With nothing available (failure).
}

TestRewriteDriverFactory* RewriteTestBase::MakeTestFactory() {
  return new TestRewriteDriverFactory(rewrite_test_base_process_context,
                                      GTestTempDir(), &mock_url_fetcher_);
}

// Test that a resource can be served from a new server that has not yet
// been constructed.
void RewriteTestBase::ServeResourceFromNewContext(
    const GoogleString& resource_url,
    const StringPiece& expected_content) {
  // New objects for the new server.
  SimpleStats stats(factory_->thread_system());
  scoped_ptr<TestRewriteDriverFactory> new_factory(MakeTestFactory());
  TestRewriteDriverFactory::InitStats(&stats);
  new_factory->SetUseTestUrlNamer(factory_->use_test_url_namer());
  new_factory->SetStatistics(&stats);
  ServerContext* new_server_context = new_factory->CreateServerContext();
  new_server_context->set_hasher(server_context_->hasher());
  RewriteOptions* new_options = options_->Clone();
  server_context_->ComputeSignature(new_options);
  RewriteDriver* new_rewrite_driver = MakeDriver(new_server_context,
                                                 new_options);
  RequestHeaders request_headers;
  PopulateRequestHeaders(&request_headers);
  new_rewrite_driver->SetRequestHeaders(request_headers);

  new_factory->SetupWaitFetcher();

  MockMessageHandler* handler = new_factory->mock_message_handler();
  handler->AddPatternToSkipPrinting(kMessagePatternShrinkImage);

  // TODO(sligocki): We should set default request headers.
  ExpectStringAsyncFetch response_contents(true, CreateRequestContext());

  // Check that we don't already have it in cache.
  HTTPValue value;
  ResponseHeaders response_headers;
  EXPECT_EQ(kNotFoundResult, HttpBlockingFind(
      resource_url, new_server_context->http_cache(), &value,
      &response_headers));
  // Initiate fetch.
  EXPECT_TRUE(new_rewrite_driver->FetchResource(
      resource_url, &response_contents));

  // Content should not be set until we call the callback.
  EXPECT_FALSE(response_contents.done());
  EXPECT_EQ("", response_contents.buffer());

  // After we call the callback, it should be correct.
  new_factory->CallFetcherCallbacksForDriver(new_rewrite_driver);
  // Since CallFetcherCallbacksForDriver waits for completion, we
  // can safely call Clear() on the driver now.
  new_rewrite_driver->Clear();
  EXPECT_TRUE(response_contents.done());
  EXPECT_STREQ(expected_content, response_contents.buffer());

  // Check that stats say we took the construct resource path.
  RewriteStats* new_stats = new_factory->rewrite_stats();
  EXPECT_EQ(0, new_stats->cached_resource_fetches()->Get());
  // We should construct at least one resource, and maybe more if the
  // output resource was produced by multiple filters (e.g. JS minimize
  // then combine).
  EXPECT_LE(1, new_stats->succeeded_filter_resource_fetches()->Get());
  EXPECT_EQ(0, new_stats->failed_filter_resource_fetches()->Get());

  // Make sure to shut the new worker down before we hit ~RewriteDriver for
  // new_rewrite_driver.
  new_factory->ShutDown();
  delete new_rewrite_driver;
}

GoogleString RewriteTestBase::AbsolutifyUrl(
    const StringPiece& resource_name) {
  GoogleString name;
  if (resource_name.starts_with("http://") ||
      resource_name.starts_with("https://")) {
    resource_name.CopyToString(&name);
  } else {
    name = StrCat(kTestDomain, resource_name);
  }
  return name;
}

void RewriteTestBase::DefaultResponseHeaders(
    const ContentType& content_type, int64 ttl_sec,
    ResponseHeaders* response_headers) {
  SetDefaultLongCacheHeaders(&content_type, response_headers);
  response_headers->SetDateAndCaching(
      timer()->NowMs(), ttl_sec * Timer::kSecondMs);
  response_headers->ComputeCaching();
}

// Initializes a resource for mock fetching.
void RewriteTestBase::SetResponseWithDefaultHeaders(
    const StringPiece& resource_name,
    const ContentType& content_type,
    const StringPiece& content,
    int64 ttl_sec) {
  GoogleString url = AbsolutifyUrl(resource_name);
  ResponseHeaders response_headers;
  DefaultResponseHeaders(content_type, ttl_sec, &response_headers);
  // Do not set Etag and Last-Modified headers to the constants since they make
  // conditional refreshes always succeed and aren't updated in tests when the
  // actual response is updated.
  response_headers.RemoveAll(HttpAttributes::kEtag);
  response_headers.RemoveAll(HttpAttributes::kLastModified);
  SetFetchResponse(url, response_headers, content);
}

void RewriteTestBase::SetFetchResponse404(
    const StringPiece& resource_name) {
  GoogleString name = AbsolutifyUrl(resource_name);
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeText, &response_headers);
  response_headers.SetStatusAndReason(HttpStatus::kNotFound);
  SetFetchResponse(name, response_headers, "");
}

bool RewriteTestBase::LoadFile(const StringPiece& filename,
                               GoogleString* contents) {
  // We need to load a file from the testdata directory. Don't use this
  // physical filesystem for anything else, use file_system_ which can be
  // abstracted as a MemFileSystem instead.
  StdioFileSystem stdio_file_system;
  GoogleString filename_str = StrCat(GTestSrcDir(), kTestData, filename);
  return stdio_file_system.ReadFile(
      filename_str.c_str(), contents, message_handler());
}

void RewriteTestBase::AddFileToMockFetcher(
    const StringPiece& url,
    const StringPiece& filename,
    const ContentType& content_type,
    int64 ttl_sec) {
  // TODO(sligocki): There's probably a lot of wasteful copying here.

  GoogleString contents;
  ASSERT_TRUE(LoadFile(filename, &contents));
  SetResponseWithDefaultHeaders(url, content_type, contents, ttl_sec);
}

// Helper function to test resource fetching, returning true if the fetch
// succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
// on the status and EXPECT_EQ on the content.
bool RewriteTestBase::FetchResource(
    const StringPiece& path, const StringPiece& filter_id,
    const StringPiece& name, const StringPiece& ext,
    GoogleString* content, ResponseHeaders* response) {
  GoogleString url = Encode(path, filter_id, "0", name, ext);
  return FetchResourceUrl(url, content, response);
}

bool RewriteTestBase::FetchResource(
    const StringPiece& path, const StringPiece& filter_id,
    const StringPiece& name, const StringPiece& ext,
    GoogleString* content) {
  ResponseHeaders response;
  return FetchResource(path, filter_id, name, ext, content, &response);
}

bool RewriteTestBase::FetchResourceUrl(
    const StringPiece& url, GoogleString* content) {
  ResponseHeaders response;
  return FetchResourceUrl(url, content, &response);
}

bool RewriteTestBase::FetchResourceUrl(
    const StringPiece& url, GoogleString* content, ResponseHeaders* response) {
  return FetchResourceUrl(url, NULL, content, response);
}

bool RewriteTestBase::FetchResourceUrl(const StringPiece& url,
                                       RequestHeaders* request_headers,
                                       GoogleString* content,
                                       ResponseHeaders* response_headers) {
  content->clear();
  StringAsyncFetch async_fetch(request_context(), content);
  if (request_headers != NULL) {
    request_headers->Add(HttpAttributes::kAcceptEncoding,
                         HttpAttributes::kGzip);
    async_fetch.set_request_headers(request_headers);
  } else if (rewrite_driver_->request_headers() == NULL) {
    SetDriverRequestHeaders();
  }
  async_fetch.set_response_headers(response_headers);
  bool fetched = rewrite_driver_->FetchResource(url, &async_fetch);
  // Make sure we let the rewrite complete, and also wait for the driver to be
  // idle so we can reuse it safely.
  rewrite_driver_->WaitForShutDown();

  ClearRewriteDriver();

  // The callback should be called if and only if FetchResource returns true.
  EXPECT_EQ(fetched, async_fetch.done());
  if (fetched && async_fetch.success() &&
      response_headers->HasValue(HttpAttributes::kContentEncoding,
                                 HttpAttributes::kGzip)) {
    GoogleString buf;
    StringWriter writer(&buf);
    if (GzipInflater::Inflate(*content, GzipInflater::kGzip, &writer)) {
      content->swap(buf);
      response_headers->Remove(HttpAttributes::kContentEncoding,
                               HttpAttributes::kGzip);
      response_headers->Add(kPsaWasGzipped, "true");
      response_headers->ComputeCaching();
    }
  }
  return fetched && async_fetch.success();
}

void RewriteTestBase::TestServeFiles(
    const ContentType* content_type,
    const StringPiece& filter_id,
    const StringPiece& rewritten_ext,
    const StringPiece& orig_name,
    const StringPiece& orig_content,
    const StringPiece& rewritten_name,
    const StringPiece& rewritten_content) {

  GoogleString expected_rewritten_path = Encode(kTestDomain, filter_id, "0",
                                                rewritten_name, rewritten_ext);
  GoogleString content;

  // When we start, there are no mock fetchers, so we'll need to get it
  // from the cache.
  ResponseHeaders headers;
  SetDefaultLongCacheHeaders(content_type, &headers);
  HTTPCache* http_cache = server_context_->http_cache();
  http_cache->Put(expected_rewritten_path, rewrite_driver_->CacheFragment(),
                  RequestHeaders::Properties(),
                  ResponseHeaders::GetVaryOption(options()->respect_vary()),
                  &headers, rewritten_content, message_handler());
  EXPECT_EQ(0U, lru_cache()->num_hits());
  EXPECT_TRUE(FetchResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  RewriteFilter* filter = rewrite_driver_->FindFilter(filter_id);
  if (lru_cache()->IsHealthy()) {
    if (filter->ComputeOnTheFly()) {
      EXPECT_EQ(2U, lru_cache()->num_hits());
    } else {
      EXPECT_EQ(1U, lru_cache()->num_hits());
    }
  }
  EXPECT_STREQ(rewritten_content, content);

  // Now nuke the cache, get it via a fetch.
  lru_cache()->Clear();
  SetResponseWithDefaultHeaders(orig_name, *content_type,
                                orig_content, 100 /* ttl in seconds */);
  EXPECT_TRUE(FetchResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(rewritten_content, content);

  // Now we expect the cache entry to be there.
  if (!filter->ComputeOnTheFly() && lru_cache()->IsHealthy()) {
    HTTPValue value;
    ResponseHeaders response_headers;
    EXPECT_EQ(kFoundResult, HttpBlockingFind(
        expected_rewritten_path, http_cache, &value, &response_headers));
  }
}

void RewriteTestBase::ValidateFallbackHeaderSanitizationHelper(
    StringPiece filter_id, StringPiece origin_content_type, bool expect_load) {

  // Mangle the content type to make a url name by removing '/'s.
  GoogleString leafable(origin_content_type.as_string());
  GlobalReplaceSubstring("/", "-", &leafable);

  GoogleString leaf = StrCat("leaf-", leafable);
  GoogleString origin_contents = "this isn't a real file";

  ResponseHeaders origin_response_headers;
  origin_response_headers.set_major_version(1);
  origin_response_headers.set_minor_version(1);
  origin_response_headers.SetStatusAndReason(HttpStatus::kOK);
  origin_response_headers.Add(HttpAttributes::kContentType,
                              origin_content_type);

  int64 now_ms = timer()->NowMs();
  // This is a case where we do need to make some changes for security and we
  // want to be sure we make them even if no-transform is set.
  origin_response_headers.SetDateAndCaching(now_ms, 0 /* ttl */,
                                            "; no-transform");
  origin_response_headers.ComputeCaching();

  SetFetchResponse(
      AbsolutifyUrl(leaf), origin_response_headers, origin_contents);

  GoogleString resource = AbsolutifyUrl(Encode(
      "", filter_id, "0", leaf, "ignored"));

  GoogleString response_content;
  ResponseHeaders response_headers;

  if (expect_load) {
    ASSERT_TRUE(FetchResourceUrl(resource,
                                 NULL /* use default request headers */,
                                 &response_content,
                                 &response_headers));
    EXPECT_EQ(origin_contents, response_content);
    const ContentType* content_type = response_headers.DetermineContentType();
    ASSERT_TRUE(NULL != content_type);
    EXPECT_EQ(origin_content_type, content_type->mime_type());

    const char* nosniff = response_headers.Lookup1("X-Content-Type-Options");
    ASSERT_TRUE(NULL != nosniff);
    EXPECT_EQ("nosniff", GoogleString(nosniff));
  } else {
    ASSERT_FALSE(FetchResourceUrl(resource,
                                  NULL /* use default request headers */,
                                  &response_content,
                                  &response_headers));
  }
}


void RewriteTestBase::ValidateFallbackHeaderSanitization(
    StringPiece filter_id) {
  // Freeze our options.
  server_context()->ComputeSignature(options());

  // These content types will all be preserved.
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "text/css", true);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "text/javascript", true);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "application/javascript", true);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "image/jpg", true);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "image/jpeg", true);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "image/png", true);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "image/gif", true);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "image/webp", true);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "application/pdf", true);

  // All other content types will be stripped.
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "text/html", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "text/plain", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "text/xml", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "application/xml", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "image/svg", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "image/svg+xml", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "audio/mp3", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "video/mp4", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "", false);
  ValidateFallbackHeaderSanitizationHelper(
      filter_id, "invalid", false);
}

// Just check if we can fetch a resource successfully, ignore response.
bool RewriteTestBase::TryFetchResource(const StringPiece& url) {
  GoogleString contents;
  ResponseHeaders response;
  return FetchResourceUrl(url, &contents, &response);
}


RewriteTestBase::CssLink::CssLink(
    const StringPiece& url, const StringPiece& content,
    const StringPiece& media, bool supply_mock)
    : url_(url.data(), url.size()),
      content_(content.data(), content.size()),
      media_(media.data(), media.size()),
      supply_mock_(supply_mock) {
}

RewriteTestBase::CssLink::Vector::~Vector() {
  STLDeleteElements(this);
}

void RewriteTestBase::CssLink::Vector::Add(
    const StringPiece& url, const StringPiece& content,
    const StringPiece& media, bool supply_mock) {
  push_back(new CssLink(url, content, media, supply_mock));
}

bool RewriteTestBase::CssLink::DecomposeCombinedUrl(
    StringPiece base_url, GoogleString* base,
    StringVector* segments, MessageHandler* handler) {
  GoogleUrl base_gurl(base_url);
  GoogleUrl gurl(base_gurl, url_);
  bool ret = false;
  if (gurl.IsWebValid()) {
    gurl.AllExceptLeaf().CopyToString(base);
    ResourceNamer namer;
    if (namer.DecodeIgnoreHashAndSignature(gurl.LeafWithQuery()) &&
        (namer.id() == RewriteOptions::kCssCombinerId)) {
      UrlMultipartEncoder multipart_encoder;
      GoogleString segment;
      ret = multipart_encoder.Decode(namer.name(), segments, NULL, handler);
    }
  }
  return ret;
}

namespace {

// Helper class to collect CSS hrefs.
class CssCollector : public EmptyHtmlFilter {
 public:
  CssCollector(HtmlParse* html_parse,
               RewriteTestBase::CssLink::Vector* css_links)
      : css_links_(css_links) {
  }

  virtual void EndElement(HtmlElement* element) {
    HtmlElement::Attribute* href;
    const char* media;
    if (CssTagScanner::ParseCssElement(element, &href, &media)) {
      // TODO(jmarantz): collect content of the CSS files, before and
      // after combination, so we can diff.
      const char* content = "";
      css_links_->Add(href->DecodedValueOrNull(), content, media, false);
    }
  }

  virtual const char* Name() const { return "CssCollector"; }

 private:
  RewriteTestBase::CssLink::Vector* css_links_;

  DISALLOW_COPY_AND_ASSIGN(CssCollector);
};

}  // namespace

// Collects just the hrefs from CSS links into a string vector.
void RewriteTestBase::CollectCssLinks(
    const StringPiece& id, const StringPiece& html, StringVector* css_links) {
  CssLink::Vector v;
  CollectCssLinks(id, html, &v);
  for (int i = 0, n = v.size(); i < n; ++i) {
    css_links->push_back(v[i]->url_);
  }
}

// Collects all information about CSS links into a CssLink::Vector.
void RewriteTestBase::CollectCssLinks(
    const StringPiece& id, const StringPiece& html,
    CssLink::Vector* css_links) {
  HtmlParse html_parse(message_handler());
  CssCollector collector(&html_parse, css_links);
  html_parse.AddFilter(&collector);
  GoogleString dummy_url = StrCat("http://collect.css.links/", id, ".html");
  html_parse.StartParse(dummy_url);
  html_parse.ParseText(html.data(), html.size());
  html_parse.FinishParse();
}

void RewriteTestBase::SetupWriter() {
  if (!rewrite_driver_->filters_added()) {
    rewrite_driver_->AddFilters();
  }
  if (!rewrite_driver()->has_html_writer_filter()) {
    RewriteOptionsTestBase::SetupWriter();
  }
}

void RewriteTestBase::EncodePathAndLeaf(const StringPiece& id,
                                        const StringPiece& hash,
                                        const StringVector& name_vector,
                                        const StringPiece& ext,
                                        ResourceNamer* namer) {
  namer->set_id(id);
  namer->set_hash(hash);

  // We only want to encode the last path-segment of 'name'.
  // Note that this block of code could be avoided if all call-sites
  // put subdirectory info in the 'path' argument, but it turns out
  // to be a lot more convenient for tests if we allow relative paths
  // in the 'name' argument for this method, so the one-time effort of
  // teasing out the leaf and encoding that saves a whole lot of clutter
  // in, at least, CacheExtenderTest.
  //
  // Note that this can only be done for 1-element name_vectors.
  // TODO(jmarantz): Modify this to work with combining across paths.
  for (int i = 0, n = name_vector.size(); i < n; ++i) {
    const GoogleString& name = name_vector[i];
    CHECK(name.find('/') == GoogleString::npos) << "No slashes should be "
        "found in " << name << " but we found at least one.  "
        "Put it in the path";
  }

  // Note: This uses an empty context, so no custom parameters like image
  // dimensions can be passed in.
  ResourceContext dummy_context;
  ImageUrlEncoder::SetWebpAndMobileUserAgent(*rewrite_driver(), &dummy_context);
  const UrlSegmentEncoder* encoder = FindEncoder(id);
  GoogleString encoded_name;
  encoder->Encode(name_vector, &dummy_context, &encoded_name);
  namer->set_name(encoded_name);
  namer->set_ext(ext);
}

const UrlSegmentEncoder* RewriteTestBase::FindEncoder(
    const StringPiece& id) const {
  RewriteFilter* filter = rewrite_driver_->FindFilter(id);
  return (filter == NULL) ? &default_encoder_ : filter->encoder();
}

GoogleString RewriteTestBase::Encode(const StringPiece& path,
                                     const StringPiece& id,
                                     const StringPiece& hash,
                                     const StringVector& name_vector,
                                     const StringPiece& ext) {
  return EncodeWithBase(kTestDomain, path, id, hash, name_vector, ext);
}

GoogleString RewriteTestBase::EncodeNormal(
    const StringPiece& path,
    const StringPiece& id,
    const StringPiece& hash,
    const StringVector& name_vector,
    const StringPiece& ext) {
  ResourceNamer namer;
  EncodePathAndLeaf(id, hash, name_vector, ext, &namer);
  return StrCat(path, namer.Encode());
}

GoogleString RewriteTestBase::EncodeWithBase(
    const StringPiece& base,
    const StringPiece& path,
    const StringPiece& id,
    const StringPiece& hash,
    const StringVector& name_vector,
    const StringPiece& ext) {
  if (factory()->use_test_url_namer() &&
      !TestUrlNamer::UseNormalEncoding() &&
      !options()->domain_lawyer()->can_rewrite_domains() &&
      !path.empty()) {
    ResourceNamer namer;
    EncodePathAndLeaf(id, hash, name_vector, ext, &namer);
    GoogleUrl path_gurl(path);
    CHECK(path_gurl.IsWebValid());
    return TestUrlNamer::EncodeUrl(base, path_gurl.Origin(),
                                   path_gurl.PathSansLeaf(), namer);
  }

  return EncodeNormal(path, id, hash, name_vector, ext);
}

GoogleString RewriteTestBase::AddOptionsToEncodedUrl(
    const StringPiece& url, const StringPiece& options) {
  ResourceNamer namer;
  CHECK(rewrite_driver()->Decode(url, &namer));
  namer.set_options(options);
  return namer.Encode();
}

GoogleString RewriteTestBase::EncodeImage(
    int width, int height,
    StringPiece filename, StringPiece hash, StringPiece rewritten_ext) {
  // filename starts as just the leaf filename, ex: foo.png
  ResourceContext params;
  // Use width, height < 0 to indicate none set.
  if (width >= 0) {
    params.mutable_desired_image_dims()->set_width(width);
  }
  if (height >= 0) {
    params.mutable_desired_image_dims()->set_height(height);
  }

  // Encoder inserts image dimensions, ex: 10x20xfoo.png
  ImageUrlEncoder encoder;
  GoogleString encoded_name;
  encoder.Encode(MultiUrl(filename), &params, &encoded_name);

  // Namer encodes into .pagespeed. format,
  // ex: 10x20xfoo.png.pagespeed.ic.0.png
  ResourceNamer namer;
  namer.set_id("ic");
  namer.set_hash(hash);
  namer.set_name(encoded_name);
  namer.set_ext(rewritten_ext);

  return namer.Encode();
}

// Helper function which instantiates an encoder, collects the
// required arguments and calls the virtual Encode().
GoogleString RewriteTestBase::EncodeCssName(const StringPiece& name,
                                            bool supports_webp,
                                            bool can_inline) {
  CssUrlEncoder encoder;
  ResourceContext resource_context;
  resource_context.set_inline_images(can_inline);
  if (supports_webp) {
    // TODO(vchudnov): Deal with webp lossless.
    resource_context.set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
  }
  StringVector urls;
  GoogleString encoded_url;
  name.CopyToString(StringVectorAdd(&urls));
  encoder.Encode(urls, &resource_context, &encoded_url);
  return encoded_url;
}

GoogleString RewriteTestBase::ChangeSuffix(
    StringPiece old_url, bool append_new_suffix,
    StringPiece old_suffix, StringPiece new_suffix) {
  if (!StringCaseEndsWith(old_url, old_suffix)) {
    ADD_FAILURE() << "Can't seem to find old extension!";
    return GoogleString();
  }

  if (append_new_suffix) {
    return StrCat(old_url, new_suffix);
  } else {
    return StrCat(
        old_url.substr(0, old_url.length() - old_suffix.length()),
        new_suffix);
  }
}

void RewriteTestBase::SetupWaitFetcher() {
  factory_->SetupWaitFetcher();
}

void RewriteTestBase::CallFetcherCallbacks() {
  factory_->CallFetcherCallbacksForDriver(rewrite_driver_);
  rewrite_driver_->Clear();
  // Since we call Clear() on the driver, give it a new request context.
  rewrite_driver_->set_request_context(CreateRequestContext());
}

void RewriteTestBase::OtherCallFetcherCallbacks() {
  other_factory_->CallFetcherCallbacksForDriver(other_rewrite_driver_);
  // This calls Clear() on the driver, so give it a new request context.
  other_rewrite_driver_->set_request_context(CreateRequestContext());
}

void RewriteTestBase::SetRewriteOptions(RewriteOptions* opts) {
  TestRewriteOptionsManager* trom = new TestRewriteOptionsManager();
  trom->set_options(opts);
  server_context()->SetRewriteOptionsManager(trom);
}

void RewriteTestBase::SetUseManagedRewriteDrivers(
    bool use_managed_rewrite_drivers) {
  use_managed_rewrite_drivers_ = use_managed_rewrite_drivers;
}

RequestContextPtr RewriteTestBase::CreateRequestContext() {
  return RequestContext::NewTestRequestContextWithTimer(
      factory_->thread_system(), timer());
}

RewriteDriver* RewriteTestBase::MakeDriver(
    ServerContext* server_context, RewriteOptions* options) {
  // We use unmanaged drivers rather than NewCustomDriver here so
  // that _test.cc files can add options after the driver was created
  // and before the filters are added.
  //
  // TODO(jmarantz): Change call-sites to make this use a more standard flow.
  RewriteDriver* rd;
  if (!use_managed_rewrite_drivers_) {
    rd = server_context->NewUnmanagedRewriteDriver(
        NULL /* custom options, so no pool*/, options,
        CreateRequestContext());
    rd->set_externally_managed(true);
  } else {
    rd = server_context->NewCustomRewriteDriver(options,
                                                CreateRequestContext());
  }

  return rd;
}

void RewriteTestBase::TestRetainExtraHeaders(
    const StringPiece& name,
    const StringPiece& filter_id,
    const StringPiece& ext) {
  GoogleString url = AbsolutifyUrl(name);

  // Add some extra headers.
  AddToResponse(url, HttpAttributes::kEtag, "Custom-Etag");
  AddToResponse(url, "extra", "attribute");
  AddToResponse(url, HttpAttributes::kSetCookie, "Custom-Cookie");

  GoogleString content;
  ResponseHeaders response;

  GoogleString rewritten_url = Encode("", filter_id, "0", name, ext);
  ASSERT_TRUE(FetchResourceUrl(StrCat(kTestDomain, rewritten_url),
                               &content, &response));

  // Extra non-blacklisted header is preserved.
  ConstStringStarVector v;
  ASSERT_TRUE(response.Lookup("extra", &v));
  ASSERT_EQ(1U, v.size());
  EXPECT_STREQ("attribute", *v[0]);

  // Note: These tests can fail if ResourceManager::FetchResource failed to
  // rewrite the resource and instead served the original.
  // TODO(sligocki): Add a check that we successfully rewrote the resource.

  // Blacklisted headers are stripped (or changed).
  EXPECT_FALSE(response.Lookup(HttpAttributes::kSetCookie, &v));

  ASSERT_TRUE(response.Lookup(HttpAttributes::kEtag, &v));
  ASSERT_EQ(1U, v.size());
  EXPECT_STREQ("W/\"0\"", *v[0]);
}

void RewriteTestBase::ClearStats() {
  statistics()->Clear();
  if (lru_cache() != NULL) {
    lru_cache()->ClearStats();
  }
  counting_url_async_fetcher()->Clear();
  other_factory_->counting_url_async_fetcher()->Clear();
  file_system()->ClearStats();
  rewrite_driver()->set_request_context(CreateRequestContext());
}

void RewriteTestBase::ClearRewriteDriver() {
  request_attribute_names_.clear();
  request_attribute_values_.clear();
  rewrite_driver()->Clear();
  rewrite_driver()->set_request_context(CreateRequestContext());
  other_rewrite_driver()->Clear();
  other_rewrite_driver()->set_request_context(CreateRequestContext());
}

void RewriteTestBase::SetCacheDelayUs(int64 delay_us) {
  factory_->mock_time_cache()->set_delay_us(delay_us);
}

void RewriteTestBase::SetUseTestUrlNamer(bool use_test_url_namer) {
  factory_->SetUseTestUrlNamer(use_test_url_namer);
  server_context_->set_url_namer(factory_->url_namer());
  other_factory_->SetUseTestUrlNamer(use_test_url_namer);
  other_server_context_->set_url_namer(other_factory_->url_namer());
}

namespace {

class BlockingResourceCallback : public Resource::AsyncCallback {
 public:
  explicit BlockingResourceCallback(const ResourcePtr& resource)
      : Resource::AsyncCallback(resource),
        done_(false),
        success_(false) {
  }
  virtual ~BlockingResourceCallback() {}
  virtual void Done(bool lock_failure, bool resource_ok) {
    done_ = true;
    success_ = !lock_failure && resource_ok;
  }
  bool done() const { return done_; }
  bool success() const { return success_; }

 private:
  bool done_;
  bool success_;
};

class DeferredResourceCallback : public Resource::AsyncCallback {
 public:
  explicit DeferredResourceCallback(const ResourcePtr& resource)
      : Resource::AsyncCallback(resource) {
  }
  virtual ~DeferredResourceCallback() {}
  virtual void Done(bool lock_failure, bool resource_ok) {
    CHECK(!lock_failure && resource_ok);
    delete this;
  }
};

class HttpCallback : public HTTPCache::Callback {
 public:
  explicit HttpCallback(const RequestContextPtr& request_context)
      : HTTPCache::Callback(request_context, RequestHeaders::Properties()),
        done_(false),
        options_(NULL) {
  }
  virtual ~HttpCallback() {}
  virtual bool IsCacheValid(const GoogleString& key,
                            const ResponseHeaders& headers) {
    if (options_ == NULL) {
      return true;
    }
    return OptionsAwareHTTPCacheCallback::IsCacheValid(
        key, *options_, request_context(), headers);
  }
  virtual void Done(HTTPCache::FindResult find_result) {
    done_ = true;
    result_ = find_result;
  }
  virtual ResponseHeaders::VaryOption RespectVaryOnResources() const {
    return ResponseHeaders::kRespectVaryOnResources;
  }

  bool done() const { return done_; }
  HTTPCache::FindResult result() { return result_; }
  void set_options(const RewriteOptions* options) { options_ = options; }

 private:
  bool done_;
  HTTPCache::FindResult result_;
  const RewriteOptions* options_;
};

}  // namespace

bool RewriteTestBase::ReadIfCached(const ResourcePtr& resource) {
  BlockingResourceCallback callback(resource);
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      request_context(), &callback);
  CHECK(callback.done());
  if (callback.success()) {
    CHECK(resource->loaded());
  }
  return callback.success();
}

void RewriteTestBase::InitiateResourceRead(
    const ResourcePtr& resource) {
  DeferredResourceCallback* callback = new DeferredResourceCallback(resource);
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      request_context(), callback);
}

HTTPCache::FindResult RewriteTestBase::HttpBlockingFindWithOptions(
    const RewriteOptions* options,
    const GoogleString& key, HTTPCache* http_cache, HTTPValue* value_out,
    ResponseHeaders* headers) {
  HttpCallback callback(CreateRequestContext());
  if (options != NULL) {
    callback.set_options(options);
  }
  callback.set_response_headers(headers);
  http_cache->Find(
      key, rewrite_driver_->CacheFragment(), message_handler(), &callback);
  CHECK(callback.done());
  value_out->Link(callback.http_value());
  return callback.result();
}

HTTPCache::FindResult RewriteTestBase::HttpBlockingFind(
    const GoogleString& key, HTTPCache* http_cache, HTTPValue* value_out,
    ResponseHeaders* headers) {
  return HttpBlockingFindWithOptions(NULL, key, http_cache, value_out, headers);
}

HTTPCache::FindResult RewriteTestBase::HttpBlockingFindStatus(
    const GoogleString& key, HTTPCache* http_cache) {
  HTTPValue value_out;
  ResponseHeaders response_headers;
  return HttpBlockingFind(key, http_cache, &value_out, &response_headers);
}

void RewriteTestBase::SetMimetype(const StringPiece& mimetype) {
  rewrite_driver()->set_response_headers_ptr(&response_headers_);
  response_headers_.Add(HttpAttributes::kContentType, mimetype);
  response_headers_.ComputeCaching();
}

void RewriteTestBase::SetupSharedCache() {
  other_server_context_->set_http_cache(
      new HTTPCache(factory_->delay_cache(), factory_->timer(),
                    factory_->hasher(), factory_->statistics()));
  other_server_context_->set_metadata_cache(factory_->delay_cache());
  // Also make sure to share the timer.
  other_server_context_->set_timer(server_context_->timer());
}

void RewriteTestBase::CheckFetchFromHttpCache(
    StringPiece url,
    StringPiece expected_contents,
    int64 expected_expiration_ms) {
  GoogleString contents;
  ResponseHeaders response;
  ClearStats();
  EXPECT_TRUE(FetchResourceUrl(url, &contents, &response)) << url;
  EXPECT_STREQ(expected_contents, contents);
  EXPECT_EQ(expected_expiration_ms, response.CacheExpirationTimeMs());
  EXPECT_TRUE(response.IsProxyCacheable(
      RequestHeaders::Properties(),
      ResponseHeaders::GetVaryOption(options()->respect_vary()),
      ResponseHeaders::kNoValidator));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));
}

void RewriteTestBase::SetActiveServer(ActiveServerFlag server_to_use) {
  if (active_server_ != server_to_use) {
    factory_.swap(other_factory_);
    std::swap(server_context_, other_server_context_);
    std::swap(rewrite_driver_, other_rewrite_driver_);
    std::swap(options_, other_options_);
    active_server_ = server_to_use;

    // If we have just swapped from a driver with an initialized writer to one
    // without an initialized writer, we have to initialize the new one ourself
    // because HtmlParseTestBaseNoAlloc::SetupWriter initializes once only, so
    // won't do it for the new one, resulting in fetched content not going to
    // the output_ data member, causing ValidateExpected calls to fail horribly.
    if (html_writer_filter_.get() != NULL &&
        other_html_writer_filter_.get() == NULL) {
      other_html_writer_filter_.reset(new HtmlWriterFilter(html_parse()));
      other_html_writer_filter_->set_writer(&write_to_string_);
      html_parse()->AddFilter(other_html_writer_filter_.get());
    }
  }
}

void RewriteTestBase::AdvanceTimeUs(int64 delay_us) {
  mock_scheduler()->AdvanceTimeUs(delay_us);
}

void RewriteTestBase::SetTimeUs(int64 time_us) {
  mock_scheduler()->SetTimeUs(time_us);
}

void RewriteTestBase::AdjustTimeUsWithoutWakingAlarms(int64 time_us) {
  factory_->mock_timer()->SetTimeUs(time_us);
}

RequestContextPtr RewriteTestBase::request_context() {
  RequestContextPtr request_context(rewrite_driver_->request_context());
  CHECK(request_context.get() != NULL);
  return request_context;
}

const RequestTimingInfo& RewriteTestBase::timing_info() {
  return request_context()->timing_info();
}

RequestTimingInfo* RewriteTestBase::mutable_timing_info() {
  return request_context()->mutable_timing_info();
}

LoggingInfo* RewriteTestBase::logging_info() {
  return request_context()->log_record()->logging_info();
}

GoogleString RewriteTestBase::AppliedRewriterStringFromLog() {
  ScopedMutex lock(request_context()->log_record()->mutex());
  return request_context()->log_record()->AppliedRewritersString();
}

void RewriteTestBase::VerifyRewriterInfoEntry(
    AbstractLogRecord* log_record, const GoogleString& id, int url_index,
    int rewriter_info_index, int rewriter_info_size, int url_list_size,
    const GoogleString& url) {
  ScopedMutex lock(log_record->mutex());
  EXPECT_GE(log_record->logging_info()->rewriter_info_size(),
            rewriter_info_size);
  const RewriterInfo& rewriter_info =
      log_record->logging_info()->rewriter_info(rewriter_info_index);
  EXPECT_STREQ(id, rewriter_info.id());
  EXPECT_TRUE(rewriter_info.has_rewrite_resource_info());
  EXPECT_EQ(url_index,
      rewriter_info.rewrite_resource_info().original_resource_url_index());
  EXPECT_EQ(url_list_size,
            log_record->logging_info()->resource_url_info().url_size());
  EXPECT_EQ(url,
      log_record->logging_info()->resource_url_info().url(url_index));
}

bool RewriteTestBase::AddDomain(StringPiece domain) {
  bool frozen = options_->ClearSignatureForTesting();
  bool ret = options_->WriteableDomainLawyer()->AddDomain(
      domain, message_handler());
  if (frozen) {
    server_context()->ComputeSignature(options_);
  }
  return ret;
}

bool RewriteTestBase::AddOriginDomainMapping(StringPiece to_domain,
                                             StringPiece from_domain) {
  bool frozen = options_->ClearSignatureForTesting();
  bool ret = options_->WriteableDomainLawyer()->AddOriginDomainMapping(
      to_domain, from_domain, "", message_handler());
  if (frozen) {
    server_context()->ComputeSignature(options_);
  }
  return ret;
}

bool RewriteTestBase::AddRewriteDomainMapping(StringPiece to_domain,
                                              StringPiece from_domain) {
  bool frozen = options_->ClearSignatureForTesting();
  bool ret = options_->WriteableDomainLawyer()->AddRewriteDomainMapping(
      to_domain, from_domain, message_handler());
  if (frozen) {
    server_context()->ComputeSignature(options_);
  }
  return ret;
}

bool RewriteTestBase::AddShard(StringPiece domain, StringPiece shards) {
  bool frozen = options_->ClearSignatureForTesting();
  bool ret = options_->WriteableDomainLawyer()->AddShard(
      domain, shards, message_handler());
  if (frozen) {
    server_context()->ComputeSignature(options_);
  }
  return ret;
}

void RewriteTestBase::SetMockLogRecord() {
  rewrite_driver_->set_request_context(
      RequestContext::NewTestRequestContext(new MockLogRecord(
          factory()->thread_system()->NewMutex())));
}

MockLogRecord* RewriteTestBase::mock_log_record() {
  return dynamic_cast<MockLogRecord*>(rewrite_driver_->log_record());
}

GoogleString RewriteTestBase::GetLazyloadScriptHtml() {
  return StrCat(
      "<script type=\"text/javascript\" data-pagespeed-no-defer>",
      LazyloadImagesFilter::GetLazyloadJsSnippet(
          options(), server_context()->static_asset_manager()),
      "</script>");
}

GoogleString RewriteTestBase::GetLazyloadPostscriptHtml() {
  return StrCat(
      "<script type=\"text/javascript\" data-pagespeed-no-defer>",
        LazyloadImagesFilter::kOverrideAttributeFunctions,
      "</script>");
}

void RewriteTestBase::SetCacheInvalidationTimestamp() {
  options()->ClearSignatureForTesting();
  // Make sure the time is different, since otherwise we may end up with
  // re-fetches resulting in re-inserts rather than inserts.
  AdvanceTimeMs(Timer::kSecondMs);
  int64 now_ms = timer()->NowMs();
  options()->UpdateCacheInvalidationTimestampMs(now_ms);
  options()->ComputeSignature();
  AdvanceTimeMs(Timer::kSecondMs);
}

void RewriteTestBase::SetCacheInvalidationTimestampForUrl(
    StringPiece url, bool ignores_metadata_and_pcache) {
  options()->ClearSignatureForTesting();
  // Make sure the time is different, since otherwise we may end up with
  // re-fetches resulting in re-inserts rather than inserts.
  AdvanceTimeMs(Timer::kSecondMs);
  options()->AddUrlCacheInvalidationEntry(url, timer()->NowMs(),
                                          ignores_metadata_and_pcache);
  options()->ComputeSignature();
  AdvanceTimeMs(Timer::kSecondMs);
}

void RewriteTestBase::EnableCachePurge() {
  options()->ClearSignatureForTesting();
  options()->set_enable_cache_purge(true);
  options()->ComputeSignature();
}

void RewriteTestBase::EnableDebug() {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->ComputeSignature();
}

GoogleString RewriteTestBase::DebugMessage(StringPiece url) {
  GoogleString result(debug_message_);
  GoogleUrl test_domain(kTestDomain);
  GoogleUrl gurl(test_domain, url);
  if (gurl.IsAnyValid()) {
    // Resolves vs test_domain to a valid absolute url.  Use that.
    GlobalReplaceSubstring("%url%", gurl.Spec(), &result);
  } else {
    // Couldn't resolve to a valid url, just use string as passed in.
    GlobalReplaceSubstring("%url%", url, &result);
  }
  return result;
}

GoogleString RewriteTestBase::ExpectedNonce() {
  GoogleString result;
  StringPiece nonce_piece(reinterpret_cast<char*>(&expected_nonce_),
                          sizeof(expected_nonce_));
  Web64Encode(nonce_piece, &result);
  result.resize(11);
  ++expected_nonce_;
  return result;
}

const ProcessContext& RewriteTestBase::process_context() {
  return rewrite_test_base_process_context;
}

int RewriteTestBase::TimedValue(StringPiece name) {
  return statistics()->GetTimedVariable(name)->Get(TimedVariable::START);
}

void RewriteTestBase::DisableGzip() {
  bool was_frozen = options()->ClearSignatureForTesting();
  options()->set_http_cache_compression_level(0);
  if (was_frozen) {
    server_context()->ComputeSignature(options());
  }
  was_frozen = other_options()->ClearSignatureForTesting();
  other_options()->set_http_cache_compression_level(0);
  if (was_frozen) {
    other_server_context()->ComputeSignature(other_options());
  }
  http_cache()->SetCompressionLevel(0);
  other_server_context()->http_cache()->SetCompressionLevel(0);
}

bool RewriteTestBase::WasGzipped(const ResponseHeaders& response_headers) {
  // Content-Encoding is stripped by FetchResourceUrl, but
  // x-psa-was-gzipped is retained, so we use it as a signal that
  // gzip occurred.
  return response_headers.Has(kPsaWasGzipped);
}

}  // namespace net_instaweb
