// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/metadata_recorder.h"

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

bool operator==(const base::ProfileBuilder::MetadataItem& lhs,
                const base::ProfileBuilder::MetadataItem& rhs) {
  return lhs.name_hash == rhs.name_hash && lhs.value == rhs.value;
}

bool operator<(const base::ProfileBuilder::MetadataItem& lhs,
               const base::ProfileBuilder::MetadataItem& rhs) {
  return lhs.name_hash < rhs.name_hash;
}

TEST(MetadataRecorderTest, GetItems_Empty) {
  MetadataRecorder recorder;
  base::ProfileBuilder::MetadataItemArray items;

  size_t item_count = recorder.CreateMetadataProvider()->GetItems(&items);

  ASSERT_EQ(0u, item_count);
}

TEST(MetadataRecorderTest, Set_NewNameHash) {
  MetadataRecorder recorder;

  recorder.Set(10, 20);

  base::ProfileBuilder::MetadataItemArray items;
  size_t item_count;
  {
    item_count = recorder.CreateMetadataProvider()->GetItems(&items);
    ASSERT_EQ(1u, item_count);
    ASSERT_EQ(10u, items[0].name_hash);
    ASSERT_EQ(20, items[0].value);
  }

  recorder.Set(20, 30);

  {
    item_count = recorder.CreateMetadataProvider()->GetItems(&items);
    ASSERT_EQ(2u, item_count);
    ASSERT_EQ(20u, items[1].name_hash);
    ASSERT_EQ(30, items[1].value);
  }
}

TEST(MetadataRecorderTest, Set_ExistingNameNash) {
  MetadataRecorder recorder;
  recorder.Set(10, 20);
  recorder.Set(10, 30);

  base::ProfileBuilder::MetadataItemArray items;
  size_t item_count = recorder.CreateMetadataProvider()->GetItems(&items);
  ASSERT_EQ(1u, item_count);
  ASSERT_EQ(10u, items[0].name_hash);
  ASSERT_EQ(30, items[0].value);
}

TEST(MetadataRecorderTest, Set_ReAddRemovedNameNash) {
  MetadataRecorder recorder;
  base::ProfileBuilder::MetadataItemArray items;
  std::vector<base::ProfileBuilder::MetadataItem> expected;
  for (size_t i = 0; i < items.size(); ++i) {
    expected.push_back(base::ProfileBuilder::MetadataItem{i, 0});
    recorder.Set(i, 0);
  }

  // By removing an item from a full recorder, re-setting the same item, and
  // verifying that the item is returned, we can verify that the recorder is
  // reusing the inactive slot for the same name hash instead of trying (and
  // failing) to allocate a new slot.
  recorder.Remove(3);
  recorder.Set(3, 0);

  size_t item_count = recorder.CreateMetadataProvider()->GetItems(&items);
  EXPECT_EQ(items.size(), item_count);
  ASSERT_THAT(expected, ::testing::UnorderedElementsAreArray(items));
}

TEST(MetadataRecorderTest, Set_AddPastMaxCount) {
  MetadataRecorder recorder;
  base::ProfileBuilder::MetadataItemArray items;
  for (size_t i = 0; i < items.size(); ++i) {
    recorder.Set(i, 0);
  }

  // This should fail silently.
  recorder.Set(items.size(), 0);
}

TEST(MetadataRecorderTest, Remove) {
  MetadataRecorder recorder;
  recorder.Set(10, 20);
  recorder.Set(30, 40);
  recorder.Set(50, 60);
  recorder.Remove(30);

  base::ProfileBuilder::MetadataItemArray items;
  size_t item_count = recorder.CreateMetadataProvider()->GetItems(&items);
  ASSERT_EQ(2u, item_count);
  ASSERT_EQ(10u, items[0].name_hash);
  ASSERT_EQ(20, items[0].value);
  ASSERT_EQ(50u, items[1].name_hash);
  ASSERT_EQ(60, items[1].value);
}

TEST(MetadataRecorderTest, Remove_DoesntExist) {
  MetadataRecorder recorder;
  recorder.Set(10, 20);
  recorder.Remove(20);

  base::ProfileBuilder::MetadataItemArray items;
  size_t item_count = recorder.CreateMetadataProvider()->GetItems(&items);
  ASSERT_EQ(1u, item_count);
  ASSERT_EQ(10u, items[0].name_hash);
  ASSERT_EQ(20, items[0].value);
}

TEST(MetadataRecorderTest, ReclaimInactiveSlots) {
  MetadataRecorder recorder;

  std::set<base::ProfileBuilder::MetadataItem> items_set;
  // Fill up the metadata map.
  for (size_t i = 0; i < base::ProfileBuilder::MAX_METADATA_COUNT; ++i) {
    recorder.Set(i, i);
    items_set.insert(base::ProfileBuilder::MetadataItem{i, i});
  }

  // Remove every fourth entry to fragment the data.
  size_t entries_removed = 0;
  for (size_t i = 3; i < base::ProfileBuilder::MAX_METADATA_COUNT; i += 4) {
    recorder.Remove(i);
    ++entries_removed;
    items_set.erase(base::ProfileBuilder::MetadataItem{i, i});
  }

  // Ensure that the inactive slots are reclaimed to make room for more entries.
  for (size_t i = 1; i <= entries_removed; ++i) {
    recorder.Set(i * 100, i * 100);
    items_set.insert(base::ProfileBuilder::MetadataItem{i * 100, i * 100});
  }

  base::ProfileBuilder::MetadataItemArray items_arr;
  std::copy(items_set.begin(), items_set.end(), items_arr.begin());

  base::ProfileBuilder::MetadataItemArray recorder_items;
  size_t recorder_item_count =
      recorder.CreateMetadataProvider()->GetItems(&recorder_items);
  ASSERT_EQ(recorder_item_count, base::ProfileBuilder::MAX_METADATA_COUNT);
  ASSERT_THAT(recorder_items, ::testing::UnorderedElementsAreArray(items_arr));
}

TEST(MetadataRecorderTest, MetadataSlotsUsedUmaHistogram) {
  MetadataRecorder recorder;
  base::HistogramTester histogram_tester;

  for (size_t i = 0; i < base::ProfileBuilder::MAX_METADATA_COUNT; ++i) {
    recorder.Set(i * 10, i * 100);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples("StackSamplingProfiler.MetadataSlotsUsed"),
      testing::ElementsAre(Bucket(1, 1), Bucket(2, 1), Bucket(3, 1),
                           Bucket(4, 1), Bucket(5, 1), Bucket(6, 1),
                           Bucket(7, 1), Bucket(8, 2), Bucket(10, 2),
                           Bucket(12, 2), Bucket(14, 3), Bucket(17, 3),
                           Bucket(20, 4), Bucket(24, 5), Bucket(29, 5),
                           Bucket(34, 6), Bucket(40, 8), Bucket(48, 3)));
}

}  // namespace base
