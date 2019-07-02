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


#include "net/instaweb/rewriter/public/association_transformer.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/css_url_counter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

// Outside of anonymous namespace to support friend declaration.
class DummyResource : public Resource {
 public:
  DummyResource() : Resource() {}
  virtual ~DummyResource() {}

  void set_url(const StringPiece& url) {
    url_ = url.as_string();
  }
  virtual GoogleString url() const { return url_; }

  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               const RequestContextPtr& request_context,
                               AsyncCallback* callback) {
    callback->Done(false, false);
  }

  virtual bool UseHttpCache() const { return false; }

 private:
  GoogleString url_;

  DISALLOW_COPY_AND_ASSIGN(DummyResource);
};

namespace {

class DummyTransformer : public CssTagScanner::Transformer {
 public:
  DummyTransformer() {}
  virtual ~DummyTransformer() {}

  virtual TransformStatus Transform(GoogleString* str) {
    *str = StrCat("Dummy:", *str);
    return kSuccess;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyTransformer);
};

}  // namespace

class AssociationTransformerTest : public ::testing::Test {
 protected:
  AssociationTransformerTest()
      : thread_system_(Platform::CreateThreadSystem()) {
    RewriteOptions::Initialize();
    options_.reset(new RewriteOptions(thread_system_.get()));
    options_->ComputeSignature();
  }

  ~AssociationTransformerTest() {
    RewriteOptions::Terminate();
  }

  template <class T>
  void ExpectValue(const std::map<GoogleString, T>& map,
                   const StringPiece& key, const T& expected_value) {
    typename std::map<GoogleString, T>::const_iterator iter =
        map.find(key.as_string());
    ASSERT_NE(map.end(), iter) << "map does not have key " << key;
    EXPECT_EQ(expected_value, iter->second)
        << "map[\"" << key << "\"] not as expected";
  }

  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<RewriteOptions> options_;
};

TEST_F(AssociationTransformerTest, TransformsCorrectly) {
  const char css_template[] =
      "blah fwe.fwei ofe w {{{ "
      "url('%s') fafwe"
      "@import '%s';829hqbr23b"
      "url()"  // Empty URLs are left alone.
      "url(%s)"
      "url(%s)"
      "url(%s)";
  const GoogleString css_before = StringPrintf(
      css_template, "image.gif", "before.css", "http://example.com/before.css",
      "http://other.org/foo.ttf", "data:text/plain,Foobar");

  GoogleUrl base_url("http://example.com/");
  NullMessageHandler handler;
  CssUrlCounter url_counter(&base_url, &handler);
  DummyTransformer backup_trans;
  AssociationTransformer trans(&base_url, options_.get(), &backup_trans,
                               &handler);

  // Run first pass.
  EXPECT_TRUE(url_counter.Count(css_before));

  // Check that 1 URL was discovered and absolutified correctly.
  EXPECT_EQ(4, url_counter.url_counts().size());
  ExpectValue(url_counter.url_counts(), "http://example.com/image.gif", 1);
  ExpectValue(url_counter.url_counts(), "http://example.com/before.css", 2);
  ExpectValue(url_counter.url_counts(), "http://other.org/foo.ttf", 1);
  ExpectValue(url_counter.url_counts(), "data:text/plain,Foobar", 1);

  // Provide URL association.
  DummyResource* resource = new DummyResource;
  ResourcePtr resource_ptr(resource);
  ResourceSlotPtr slot(new AssociationSlot(
      resource_ptr, trans.map(), "http://example.com/before.css"));
  resource->set_url("http://example.com/after.css");
  slot->Render();

  // Check that the association was registered.
  EXPECT_EQ(1, trans.map()->size());
  ExpectValue<GoogleString>(*trans.map(), "http://example.com/before.css",
                            "http://example.com/after.css");

  // Run second pass.
  GoogleString out;
  StringWriter out_writer(&out);
  EXPECT_TRUE(CssTagScanner::TransformUrls(css_before, &out_writer, &trans,
                                           &handler));

  // Check that contents was rewritten correctly.
  const GoogleString css_after = StringPrintf(
      css_template,
      // image.gif did not have an association set, so it was passed to
      // DummyTransformer.
      "Dummy:image.gif",
      // before.css was rewritten in both places to after.css.
      // The first one stays relative and the second stays absolute.
      "after.css",
      "http://example.com/after.css",
      // Passed through DummyTransformer.
      "Dummy:http://other.org/foo.ttf",
      "Dummy:data:text/plain,Foobar");
  EXPECT_EQ(css_after, out);
}

TEST_F(AssociationTransformerTest, FailsOnInvalidUrl) {
  const char css_before[] = "url(////)";

  GoogleUrl base_url("http://example.com/");
  DummyTransformer backup_trans;
  NullMessageHandler handler;
  AssociationTransformer trans(&base_url, options_.get(), &backup_trans,
                               &handler);

  // Transform fails because there is an invalid URL.
  GoogleString out;
  StringWriter out_writer(&out);
  EXPECT_FALSE(CssTagScanner::TransformUrls(css_before, &out_writer, &trans,
                                            &handler));
}

}  // namespace net_instaweb
