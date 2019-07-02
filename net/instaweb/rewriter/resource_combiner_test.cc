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

//
// Unit tests for ResourceCombiner.
//
// TestCombineFilter::TestCombiner inherits off ResourceCombiner and
// provides overrides with easily testable behavior.
// TestCombineFilter is used to hook TestCombiner up with the framework.
// ResourceCombinerTest is the test fixture.

#include "net/instaweb/rewriter/public/resource_combiner.h"


#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/url_multipart_encoder.h"
#include "pagespeed/kernel/util/url_segment_encoder.h"

namespace net_instaweb {

namespace {

const char kTestCombinerId[] = "tc";
const char kTestCombinerExt[] = "tcc";
const char kTestPiece1[] = "piece1.tcc";
const char kTestPiece2[] = "piece2.tcc";
const char kTestPiece3[] = "piece3.tcc";
const char kPathPiece[]  = "path/piece.tcc";
const char kNoSuchPiece[] = "nopiece.tcc";
const char kVetoPiece[] = "veto.tcc";
const char kVetoText[] = "veto";

const char kPathCombined[] = "path,_piece.tcc+piece1.tcc";

// TestCombineFilter exists to connect up TestCombineFilter::TestCombiner with
// the normal fetch framework.
class TestCombineFilter : public RewriteFilter {
 public:
  // TestCombiner helps us test two subclass hooks:
  // 1) Preventing combinations based on content --- it vetoes resources with
  //    content equal to kVetoText
  // 2) Altering content of documents when combining --- it terminates each
  //    input's contents with a  | character.
  class TestCombiner : public ResourceCombiner {
   public:
    explicit TestCombiner(RewriteDriver* driver, RewriteFilter* filter)
        : ResourceCombiner(driver, kTestCombinerExt, filter) {
    }

    // Provides the test access to the protected method so we can
    // directly test combining without setting up a complete filter
    // with a RewriteContext.
    OutputResourcePtr TestCombine(MessageHandler* handler) {
      return Combine(handler);
    }

   protected:
    virtual bool WritePiece(int index, int num_pieces, const Resource* input,
                            OutputResource* combination, Writer* writer,
                            MessageHandler* handler) {
      ResourceCombiner::WritePiece(index, num_pieces, input,
                                   combination, writer, handler);
      writer->Write("|", handler);
      return true;
    }

   private:
    virtual const ContentType* CombinationContentType() {
      return &kContentTypeText;
    }

    virtual bool ResourceCombinable(Resource* resource,
                                    GoogleString* failure_reason,
                                    MessageHandler* /*handler*/) {
      EXPECT_TRUE(resource->HttpStatusOk());
      if (resource->ExtractUncompressedContents() == kVetoText) {
        *failure_reason = "Contents match veto text";
        return false;
      }
      return true;
    }
  };

  explicit TestCombineFilter(RewriteDriver* driver)
      : RewriteFilter(driver),
        combiner_(driver, this) {
  }

  virtual void StartDocumentImpl() {
    combiner_.Reset();
  }

  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual const char* Name() const { return "TestCombine"; }
  virtual const char* id() const { return kTestCombinerId; }

  TestCombineFilter::TestCombiner* combiner() { return &combiner_; }
  virtual const UrlSegmentEncoder* encoder() const { return &encoder_; }

 private:
  TestCombineFilter::TestCombiner combiner_;
  UrlMultipartEncoder encoder_;
};

}  // namespace

// Test fixture.
class ResourceCombinerTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();

    filter_ = new TestCombineFilter(rewrite_driver());
    AddRewriteFilter(filter_);
    AddOtherRewriteFilter(new TestCombineFilter(other_rewrite_driver()));

    // Make sure to set the domain so we authorize fetches.
    SetBaseUrlForFetch(kTestDomain);

    MockResource(kTestPiece1, "piece1", 10000);
    MockResource(kTestPiece2, "piec2", 20000);
    MockResource(kTestPiece3, "pie3", 30000);
    MockResource(kPathPiece, "path", 30000);
    MockResource(kVetoPiece, kVetoText, 30000);
    SetFetchResponse404(kNoSuchPiece);

    partnership_ = filter_->combiner();
  }

  GoogleString AbsoluteUrl(const char* relative) {
    return StrCat(kTestDomain, relative);
  }

  // Create a resource with given data and TTL
  void MockResource(const char* rel_path, const StringPiece& data, int64 ttl) {
    SetResponseWithDefaultHeaders(rel_path, kContentTypeText, data, ttl);
  }

  // Adds a new response to header to the mock fetch system for a relative URL.
  // TODO(jmarantz): standardize kTestDomain better so this can be
  // usefully promoted to RewriteTestBase.
  void AddHeader(StringPiece relative_url, StringPiece name,
                 StringPiece value) {
    AddToResponse(StrCat(kTestDomain, relative_url), name, value);
  }

  enum FetchFlags {
    kFetchNormal = 0,
    kFetchAsync  = 1,
  };

  // Fetches a resource, optionally permitting asynchronous loading (delayed
  // invocation and fetches that may fail. Returns whether succeeded
  bool FetchResource(const StringPiece& url, GoogleString* content, int flags) {
    if (flags & kFetchAsync) {
      SetupWaitFetcher();
    }

    // TODO(morlovich): This is basically copy-paste from FetchResourceUrl.
    content->clear();
    StringAsyncFetch callback(CreateRequestContext(), content);
    bool fetched = rewrite_driver()->FetchResource(url, &callback);

    if (!fetched) {
      return false;
    }

    if (flags & kFetchAsync) {
      CallFetcherCallbacks();
    }

    rewrite_driver()->WaitForCompletion();
    EXPECT_TRUE(callback.done());
    return callback.success();
  }

  // Makes sure that the resource at given position in the partnership_
  // is valid and matches the expected URL.
  void VerifyResource(int pos, const char* url) {
    EXPECT_TRUE(partnership_->resources()[pos]->HttpStatusOk());
    EXPECT_EQ(AbsoluteUrl(url), partnership_->resources()[pos]->url());
  }

  // Check that we have the expected number of things in the partnership
  void VerifyUrlCount(int num_expected) {
    ASSERT_EQ(num_expected, partnership_->num_urls());
    ASSERT_EQ(num_expected, partnership_->resources().size());
  }

  // Check to make sure we are within various URL limits
  void VerifyLengthLimits() {
    EXPECT_LE(LeafLength(partnership_->UrlSafeId().length()),
              options()->max_url_segment_size() - UrlSlack());

    EXPECT_LE(partnership_->ResolvedBase().length() +
                  LeafLength(partnership_->UrlSafeId().length()),
              options()->max_url_size() - UrlSlack());
  }

  int UrlSlack() const {
    return ResourceCombiner::kUrlSlack;
  }

  GoogleString StringOfLength(int n, char fill) {
    GoogleString out;
    out.insert(0, n, fill);
    return out;
  }

  // Returns the number of characters in the leaf file name given the
  // resource name, counting what will be spent on the hash, id, etc.
  int LeafLength(int resource_len) {
    ResourceNamer namer;
    namer.set_hash(
        StringOfLength(hasher()->HashSizeInChars(), '#'));
    namer.set_name(StringOfLength(resource_len, 'P'));
    namer.set_id(kTestCombinerId);
    namer.set_ext("tcc");
    return namer.Encode().length();
  }

  bool AddResource(const StringPiece& url, MessageHandler* handler) {
    // See if we have the source loaded, or start loading it.
    bool unused;
    ResourcePtr resource(filter_->CreateInputResource(
        url, RewriteDriver::InputRole::kUnknown, &unused));
    bool ret = false;

    if (resource.get() == NULL) {
      // Resource is not creatable, and never will be.
      handler->MessageS(kInfo, "Cannot combine: null resource");
      return ret;
    }

    if (!ReadIfCached(resource)) {
      // Resource is not cached, but may be soon.
      handler->MessageS(kInfo, "Cannot combine: not cached");
      return ret;
    }

    if (!resource->HttpStatusOk()) {
      // Resource is not valid, but may be someday.
      // TODO(sligocki): Perhaps we should follow redirects.
      handler->MessageS(kInfo, "Cannot combine: invalid contents");
      return ret;
    }

    return partnership_->AddResourceNoFetch(resource, handler).value;
  }

  void StartPartnership() {
    EXPECT_EQ(0, partnership_->num_urls());
    EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
    EXPECT_EQ(1, partnership_->num_urls());
    EXPECT_TRUE(AddResource(kTestPiece2, &message_handler_));
    EXPECT_EQ(2, partnership_->num_urls());
    EXPECT_TRUE(AddResource(kTestPiece3, &message_handler_));
    EXPECT_EQ("piece1.tcc+piece2.tcc+piece3.tcc", partnership_->UrlSafeId());
  }

  TestCombineFilter* filter_;  // owned by the rewrite_driver_.
  TestCombineFilter::TestCombiner* partnership_;  // owned by the filter_
};

TEST_F(ResourceCombinerTest, TestPartnershipBasic) {
  // Make sure we're actually combining names and filling in the
  // data arrays if everything is available.

  StartPartnership();
  VerifyUrlCount(3);
  VerifyResource(0, kTestPiece1);
  VerifyResource(1, kTestPiece2);
  VerifyResource(2, kTestPiece3);
}

TEST_F(ResourceCombinerTest, TestIncomplete1) {
  // Test with the first URL incomplete - nothing should get added
  EXPECT_FALSE(AddResource(kNoSuchPiece, &message_handler_));
  VerifyUrlCount(0);
}

TEST_F(ResourceCombinerTest, TestIncomplete2) {
  // Test with the second URL incomplete. Should include the first one.
  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  EXPECT_FALSE(AddResource(kNoSuchPiece, &message_handler_));
  EXPECT_EQ(kTestPiece1, partnership_->UrlSafeId());

  VerifyUrlCount(1);
  VerifyResource(0, kTestPiece1);
}

TEST_F(ResourceCombinerTest, TestIncomplete3) {
  // Now with the third one incomplete. Two should be in the partnership
  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  EXPECT_TRUE(AddResource(kTestPiece2, &message_handler_));
  EXPECT_FALSE(AddResource(kNoSuchPiece, &message_handler_));
  EXPECT_EQ("piece1.tcc+piece2.tcc", partnership_->UrlSafeId());

  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece1);
  VerifyResource(1, kTestPiece2);
}

TEST_F(ResourceCombinerTest, TestRemove) {
  // Add one resource, remove it, and then re-add a few.
  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kTestPiece1);

  partnership_->RemoveLastResource();
  VerifyUrlCount(0);

  EXPECT_TRUE(AddResource(kTestPiece2, &message_handler_));
  EXPECT_TRUE(AddResource(kTestPiece3, &message_handler_));
  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece2);
  VerifyResource(1, kTestPiece3);
  EXPECT_EQ("piece2.tcc+piece3.tcc", partnership_->UrlSafeId());
}

TEST_F(ResourceCombinerTest, TestRemoveFrom3) {
  // Add three resources, remove 1
  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  EXPECT_TRUE(AddResource(kTestPiece2, &message_handler_));
  EXPECT_TRUE(AddResource(kTestPiece3, &message_handler_));

  VerifyUrlCount(3);
  VerifyResource(0, kTestPiece1);
  VerifyResource(1, kTestPiece2);
  VerifyResource(2, kTestPiece3);
  EXPECT_EQ("piece1.tcc+piece2.tcc+piece3.tcc", partnership_->UrlSafeId());

  partnership_->RemoveLastResource();
  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece1);
  VerifyResource(1, kTestPiece2);
  EXPECT_EQ("piece1.tcc+piece2.tcc", partnership_->UrlSafeId());
}

TEST_F(ResourceCombinerTest, TestAddBroken) {
  // Test with the second URL broken enough for CreateInputResource to fail
  // (due to unknown protocol). In that case, we should just include the first
  // URL in the combination.
  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  EXPECT_FALSE(AddResource("slwy://example.com/", &message_handler_));
  EXPECT_EQ(kTestPiece1, partnership_->UrlSafeId());

  VerifyUrlCount(1);
  VerifyResource(0, kTestPiece1);
}

TEST_F(ResourceCombinerTest, TestVeto) {
  // Make sure a vetoed resource stops the combination
  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  EXPECT_TRUE(AddResource(kTestPiece2, &message_handler_));
  EXPECT_FALSE(AddResource(kVetoPiece, &message_handler_));
  EXPECT_EQ("piece1.tcc+piece2.tcc", partnership_->UrlSafeId());

  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece1);
  VerifyResource(1, kTestPiece2);
}

TEST_F(ResourceCombinerTest, TestRebase) {
  // A very basic test for re-resolving fragment when base changes
  EXPECT_TRUE(AddResource(kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  EXPECT_EQ(StrCat(kTestDomain, "path/"), partnership_->ResolvedBase());
  VerifyResource(0, kPathPiece);

  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  EXPECT_EQ(kPathCombined, partnership_->UrlSafeId());
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece);
  VerifyResource(1, kTestPiece1);
  EXPECT_EQ(kTestDomain, partnership_->ResolvedBase());
}

TEST_F(ResourceCombinerTest, TestRebaseRemove) {
  // Here the first item we add is: path/piece.tcc, while the second one
  // is piece1.tcc. This means after the two items our state should be
  // roughly 'path/piece.tcc and piece1.tcc in /', while after backing out
  // the last one it should be 'piece.tcc in path/'. This tests makes
  // sure we do this.
  EXPECT_TRUE(AddResource(kPathPiece, &message_handler_));

  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  EXPECT_EQ(kTestDomain, partnership_->ResolvedBase());

  partnership_->RemoveLastResource();
  VerifyUrlCount(1);
  EXPECT_EQ(StrCat(kTestDomain, "path/"), partnership_->ResolvedBase());
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyResource(0, kPathPiece);
}

TEST_F(ResourceCombinerTest, TestRebaseRemoveAdd) {
  // As above, but also add in an additional entry to see that handling of
  // different paths still works.
  EXPECT_TRUE(AddResource(kPathPiece, &message_handler_));

  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));

  partnership_->RemoveLastResource();
  VerifyUrlCount(1);
  EXPECT_EQ(StrCat(kTestDomain, "path/"), partnership_->ResolvedBase());
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyResource(0, kPathPiece);

  EXPECT_TRUE(AddResource(kTestPiece2, &message_handler_));
  VerifyUrlCount(2);
  EXPECT_EQ("path,_piece.tcc+piece2.tcc", partnership_->UrlSafeId());
  EXPECT_EQ(kTestDomain, partnership_->ResolvedBase());
  VerifyResource(0, kPathPiece);
  VerifyResource(1, kTestPiece2);
}

TEST_F(ResourceCombinerTest, TestRebaseOverflow) {
  // Test to make sure that we notice when we go over the limit when
  // we rebase - we lower the segment size limit just for that.
  options()->set_max_url_segment_size(LeafLength(strlen(kPathCombined) - 1) +
                                    UrlSlack());
  EXPECT_TRUE(AddResource(kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece);

  EXPECT_FALSE(AddResource(kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece);
  VerifyLengthLimits();

  // Note that we want the base to be reverted to the previous one.
  // otherwise, we may still end up overflowed even without the new segment
  // included, just due to path addition
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
}

TEST_F(ResourceCombinerTest, TestRebaseOverflow2) {
  // Test to make sure we are exact in our size limit
  options()->set_max_url_segment_size(LeafLength(strlen(kPathCombined)) +
                                    UrlSlack());
  EXPECT_TRUE(AddResource(kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece);

  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece);
  VerifyResource(1, kTestPiece1);
  EXPECT_EQ(kPathCombined, partnership_->UrlSafeId());
  VerifyLengthLimits();
}

TEST_F(ResourceCombinerTest, TestRebaseOverflow3) {
  // Make sure that if we add url, rebase, and then rollback we
  // don't end up overlimit due to the first piece expanding
  options()->set_max_url_segment_size(LeafLength(strlen("piece.tcc")) +
                                    UrlSlack());
  EXPECT_TRUE(AddResource(kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece);

  EXPECT_FALSE(AddResource(kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece);
  VerifyLengthLimits();
}

TEST_F(ResourceCombinerTest, TestMaxUrlOverflow) {
  // Make sure we don't produce URLs bigger than the max_url_size().
  options()->set_max_url_size(
      strlen(kTestDomain) + LeafLength(strlen(kPathCombined)) + UrlSlack() - 1);
  EXPECT_TRUE(AddResource(kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece);

  EXPECT_FALSE(AddResource(kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece);
  VerifyLengthLimits();
}

TEST_F(ResourceCombinerTest, TestMaxUrlOverflow2) {
  // This one is just right
  options()->set_max_url_size(
      strlen(kTestDomain) + LeafLength(strlen(kPathCombined)) + UrlSlack());
  EXPECT_TRUE(AddResource(kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece);

  EXPECT_TRUE(AddResource(kTestPiece1, &message_handler_));
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece);
  VerifyResource(1, kTestPiece1);
  VerifyLengthLimits();
}

TEST_F(ResourceCombinerTest, TestIntersectingHeaders) {
  AddHeader(kTestPiece1, "rock", "gibralter");
  AddHeader(kTestPiece1, "flake", "snow");
  AddHeader(kTestPiece2, "rock", "gibralter");
  AddHeader(kTestPiece2, "flake", "ash");
  AddHeader(kTestPiece3, "rock", "gibralter");
  AddHeader(kTestPiece2, "flake", "dandruff");

  StartPartnership();
  OutputResourcePtr output(partnership_->TestCombine(&message_handler_));
  const ResponseHeaders& headers = *output->response_headers();
  EXPECT_TRUE(headers.Has("rock"));
  EXPECT_TRUE(headers.HasValue("rock", "gibralter"));
  EXPECT_FALSE(headers.Has("flake"));
}

}  // namespace net_instaweb
