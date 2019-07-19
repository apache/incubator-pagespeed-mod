// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sample_metadata.h"

#include "base/metrics/metrics_hashes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SampleMetadataTest, ScopedSampleMetadata) {
  base::ProfileBuilder::MetadataItemArray items;
  {
    auto get_items = GetSampleMetadataRecorder()->CreateMetadataProvider();
    ASSERT_EQ(0u, get_items->GetItems(&items));
  }

  {
    ScopedSampleMetadata m("myname", 100);

    {
      ASSERT_EQ(1u,
                GetSampleMetadataRecorder()->CreateMetadataProvider()->GetItems(
                    &items));
      EXPECT_EQ(base::HashMetricName("myname"), items[0].name_hash);
      EXPECT_EQ(100, items[0].value);
    }
  }

  {
    ASSERT_EQ(0u,
              GetSampleMetadataRecorder()->CreateMetadataProvider()->GetItems(
                  &items));
  }
}

}  // namespace base
