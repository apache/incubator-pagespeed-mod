// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/checked_iterators.h"
#include "base/stl_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pointwise;

namespace base {

TEST(SpanTest, DefaultConstructor) {
  span<int> dynamic_span;
  EXPECT_EQ(nullptr, dynamic_span.data());
  EXPECT_EQ(0u, dynamic_span.size());

  constexpr span<int, 0> static_span;
  static_assert(nullptr == static_span.data(), "");
  static_assert(0u == static_span.size(), "");
}

TEST(SpanTest, ConstructFromDataAndSize) {
  constexpr span<int> empty_span(nullptr, 0);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<int> dynamic_span(vector.data(), vector.size());
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(vector[i], dynamic_span[i]);

  span<int, 6> static_span(vector.data(), vector.size());
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, ConstructFromPointerPair) {
  constexpr span<int> empty_span(nullptr, nullptr);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<int> dynamic_span(vector.data(), vector.data() + vector.size() / 2);
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size() / 2, dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(vector[i], dynamic_span[i]);

  span<int, 3> static_span(vector.data(), vector.data() + vector.size() / 2);
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size() / 2, static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, ConstructFromConstexprArray) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};

  constexpr span<const int> dynamic_span(kArray);
  static_assert(kArray == dynamic_span.data(), "");
  static_assert(base::size(kArray) == dynamic_span.size(), "");

  static_assert(kArray[0] == dynamic_span[0], "");
  static_assert(kArray[1] == dynamic_span[1], "");
  static_assert(kArray[2] == dynamic_span[2], "");
  static_assert(kArray[3] == dynamic_span[3], "");
  static_assert(kArray[4] == dynamic_span[4], "");

  constexpr span<const int, base::size(kArray)> static_span(kArray);
  static_assert(kArray == static_span.data(), "");
  static_assert(base::size(kArray) == static_span.size(), "");

  static_assert(kArray[0] == static_span[0], "");
  static_assert(kArray[1] == static_span[1], "");
  static_assert(kArray[2] == static_span[2], "");
  static_assert(kArray[3] == static_span[3], "");
  static_assert(kArray[4] == static_span[4], "");
}

TEST(SpanTest, ConstructFromArray) {
  int array[] = {5, 4, 3, 2, 1};

  span<const int> const_span(array);
  EXPECT_EQ(array, const_span.data());
  EXPECT_EQ(base::size(array), const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(array[i], const_span[i]);

  span<int> dynamic_span(array);
  EXPECT_EQ(array, dynamic_span.data());
  EXPECT_EQ(base::size(array), dynamic_span.size());
  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(array[i], dynamic_span[i]);

  span<int, base::size(array)> static_span(array);
  EXPECT_EQ(array, static_span.data());
  EXPECT_EQ(base::size(array), static_span.size());
  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(array[i], static_span[i]);
}

TEST(SpanTest, ConstructFromStdArray) {
  // Note: Constructing a constexpr span from a constexpr std::array does not
  // work prior to C++17 due to non-constexpr std::array::data.
  std::array<int, 5> array = {{5, 4, 3, 2, 1}};

  span<const int> const_span(array);
  EXPECT_EQ(array.data(), const_span.data());
  EXPECT_EQ(array.size(), const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(array[i], const_span[i]);

  span<int> dynamic_span(array);
  EXPECT_EQ(array.data(), dynamic_span.data());
  EXPECT_EQ(array.size(), dynamic_span.size());
  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(array[i], dynamic_span[i]);

  span<int, base::size(array)> static_span(array);
  EXPECT_EQ(array.data(), static_span.data());
  EXPECT_EQ(array.size(), static_span.size());
  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(array[i], static_span[i]);
}

TEST(SpanTest, ConstructFromInitializerList) {
  std::initializer_list<int> il = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(il);
  EXPECT_EQ(il.begin(), const_span.data());
  EXPECT_EQ(il.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(il.begin()[i], const_span[i]);

  span<const int, 6> static_span(il);
  EXPECT_EQ(il.begin(), static_span.data());
  EXPECT_EQ(il.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(il.begin()[i], static_span[i]);
}

TEST(SpanTest, ConstructFromStdString) {
  std::string str = "foobar";

  span<const char> const_span(str);
  EXPECT_EQ(str.data(), const_span.data());
  EXPECT_EQ(str.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(str[i], const_span[i]);

  span<char> dynamic_span(str);
  EXPECT_EQ(str.data(), dynamic_span.data());
  EXPECT_EQ(str.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(str[i], dynamic_span[i]);

  span<char, 6> static_span(str);
  EXPECT_EQ(str.data(), static_span.data());
  EXPECT_EQ(str.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(str[i], static_span[i]);
}

TEST(SpanTest, ConstructFromConstContainer) {
  const std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(vector[i], const_span[i]);

  span<const int, 6> static_span(vector);
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, ConstructFromContainer) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(vector[i], const_span[i]);

  span<int> dynamic_span(vector);
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(vector[i], dynamic_span[i]);

  span<int, 6> static_span(vector);
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, ConvertNonConstIntegralToConst) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<int> int_span(vector.data(), vector.size());
  span<const int> const_span(int_span);
  EXPECT_EQ(int_span.size(), const_span.size());

  EXPECT_THAT(const_span, Pointwise(Eq(), int_span));

  span<int, 6> static_int_span(vector.data(), vector.size());
  span<const int, 6> static_const_span(static_int_span);
  EXPECT_THAT(static_const_span, Pointwise(Eq(), static_int_span));
}

TEST(SpanTest, ConvertNonConstPointerToConst) {
  auto a = std::make_unique<int>(11);
  auto b = std::make_unique<int>(22);
  auto c = std::make_unique<int>(33);
  std::vector<int*> vector = {a.get(), b.get(), c.get()};

  span<int*> non_const_pointer_span(vector);
  EXPECT_THAT(non_const_pointer_span, Pointwise(Eq(), vector));
  span<int* const> const_pointer_span(non_const_pointer_span);
  EXPECT_THAT(const_pointer_span, Pointwise(Eq(), non_const_pointer_span));
  // Note: no test for conversion from span<int> to span<const int*>, since that
  // would imply a conversion from int** to const int**, which is unsafe.
  //
  // Note: no test for conversion from span<int*> to span<const int* const>,
  // due to CWG Defect 330:
  // http://open-std.org/JTC1/SC22/WG21/docs/cwg_defects.html#330

  span<int*, 3> static_non_const_pointer_span(vector);
  EXPECT_THAT(static_non_const_pointer_span, Pointwise(Eq(), vector));
  span<int* const, 3> static_const_pointer_span(static_non_const_pointer_span);
  EXPECT_THAT(static_const_pointer_span,
              Pointwise(Eq(), static_non_const_pointer_span));
}

TEST(SpanTest, ConvertBetweenEquivalentTypes) {
  std::vector<int32_t> vector = {2, 4, 8, 16, 32};

  span<int32_t> int32_t_span(vector);
  span<int> converted_span(int32_t_span);
  EXPECT_EQ(int32_t_span.data(), converted_span.data());
  EXPECT_EQ(int32_t_span.size(), converted_span.size());

  span<int32_t, 5> static_int32_t_span(vector);
  span<int, 5> static_converted_span(static_int32_t_span);
  EXPECT_EQ(static_int32_t_span.data(), static_converted_span.data());
  EXPECT_EQ(static_int32_t_span.size(), static_converted_span.size());
}

TEST(SpanTest, TemplatedFirst) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.first<0>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.first<1>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.first<2>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.first<3>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(3u == subspan.size(), "");
    static_assert(3u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
    static_assert(3 == subspan[2], "");
  }
}

TEST(SpanTest, TemplatedLast) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.last<0>();
    static_assert(span.data() + 3 == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.last<1>();
    static_assert(span.data() + 2 == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(3 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.last<2>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
    static_assert(3 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.last<3>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(3u == subspan.size(), "");
    static_assert(3u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
    static_assert(3 == subspan[2], "");
  }
}

TEST(SpanTest, TemplatedSubspan) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.subspan<0>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(3u == subspan.size(), "");
    static_assert(3u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
    static_assert(3 == subspan[2], "");
  }

  {
    constexpr auto subspan = span.subspan<1>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
    static_assert(3 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.subspan<2>();
    static_assert(span.data() + 2 == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(3 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<3>();
    static_assert(span.data() + 3 == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<0, 0>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<1, 0>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<2, 0>();
    static_assert(span.data() + 2 == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<0, 1>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<1, 1>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<2, 1>();
    static_assert(span.data() + 2 == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(3 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<0, 2>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.subspan<1, 2>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
    static_assert(3 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.subspan<0, 3>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(3u == subspan.size(), "");
    static_assert(3u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
    static_assert(3 == subspan[2], "");
  }
}

TEST(SpanTest, TemplatedFirstOnDynamicSpan) {
  int array[] = {1, 2, 3};
  span<const int> span(array);

  {
    auto subspan = span.first<0>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.first<1>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.first<2>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.first<3>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    static_assert(3u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, TemplatedLastOnDynamicSpan) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.last<0>();
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.last<1>();
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.last<2>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.last<3>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    static_assert(3u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, TemplatedSubspanFromDynamicSpan) {
  int array[] = {1, 2, 3};
  span<int, 3> span(array);

  {
    auto subspan = span.subspan<0>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(3u == decltype(subspan)::extent, "");
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }

  {
    auto subspan = span.subspan<1>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan<2>();
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan<3>();
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<0, 0>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<1, 0>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<2, 0>();
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<0, 1>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.subspan<1, 1>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
  }

  {
    auto subspan = span.subspan<2, 1>();
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan<0, 2>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.subspan<1, 2>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan<0, 3>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    static_assert(3u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, First) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.first(0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.first(1);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.first(2);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.first(3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Last) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.last(0);
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.last(1);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.last(2);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.last(3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Subspan) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.subspan(0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }

  {
    auto subspan = span.subspan(1);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan(2);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan(3);
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(0, 0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(1, 0);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(2, 0);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(0, 1);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.subspan(1, 1);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
  }

  {
    auto subspan = span.subspan(2, 1);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan(0, 2);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.subspan(1, 2);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan(0, 3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(span.size(), subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Size) {
  {
    span<int> span;
    EXPECT_EQ(0u, span.size());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_EQ(3u, span.size());
  }
}

TEST(SpanTest, SizeBytes) {
  {
    span<int> span;
    EXPECT_EQ(0u, span.size_bytes());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_EQ(3u * sizeof(int), span.size_bytes());
  }
}

TEST(SpanTest, Empty) {
  {
    span<int> span;
    EXPECT_TRUE(span.empty());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_FALSE(span.empty());
  }
}

TEST(SpanTest, OperatorAt) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  static_assert(&kArray[0] == &span[0],
                "span[0] does not refer to the same element as kArray[0]");
  static_assert(&kArray[1] == &span[1],
                "span[1] does not refer to the same element as kArray[1]");
  static_assert(&kArray[2] == &span[2],
                "span[2] does not refer to the same element as kArray[2]");
  static_assert(&kArray[3] == &span[3],
                "span[3] does not refer to the same element as kArray[3]");
  static_assert(&kArray[4] == &span[4],
                "span[4] does not refer to the same element as kArray[4]");
}

TEST(SpanTest, Front) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);
  static_assert(&kArray[0] == &span.front(),
                "span.front() does not refer to the same element as kArray[0]");
}

TEST(SpanTest, Back) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);
  static_assert(&kArray[4] == &span.back(),
                "span.back() does not refer to the same element as kArray[4]");
}

TEST(SpanTest, Swap) {
  {
    static int kArray1[] = {1, 1};
    static int kArray2[] = {1, 2};
    span<const int, 2> static_span1(kArray1);
    span<const int, 2> static_span2(kArray2);

    EXPECT_EQ(kArray1, static_span1.data());
    EXPECT_EQ(kArray2, static_span2.data());

    swap(static_span1, static_span2);

    EXPECT_EQ(kArray2, static_span1.data());
    EXPECT_EQ(kArray1, static_span2.data());
  }

  {
    static int kArray1[] = {1};
    static int kArray2[] = {1, 2};
    span<const int> dynamic_span1(kArray1);
    span<const int> dynamic_span2(kArray2);

    EXPECT_EQ(kArray1, dynamic_span1.data());
    EXPECT_EQ(1u, dynamic_span1.size());

    EXPECT_EQ(kArray2, dynamic_span2.data());
    EXPECT_EQ(2u, dynamic_span2.size());

    swap(dynamic_span1, dynamic_span2);

    EXPECT_EQ(kArray2, dynamic_span1.data());
    EXPECT_EQ(2u, dynamic_span1.size());

    EXPECT_EQ(kArray1, dynamic_span2.data());
    EXPECT_EQ(1u, dynamic_span2.size());
  }
}

TEST(SpanTest, Iterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  std::vector<int> results;
  for (int i : span)
    results.emplace_back(i);
  EXPECT_THAT(results, ElementsAre(1, 6, 1, 8, 0));
}

TEST(SpanTest, ReverseIterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  EXPECT_TRUE(std::equal(std::rbegin(kArray), std::rend(kArray), span.rbegin(),
                         span.rend()));
  EXPECT_TRUE(std::equal(std::crbegin(kArray), std::crend(kArray),
                         span.crbegin(), span.crend()));
}

TEST(SpanTest, AsBytes) {
  {
    constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
    span<const uint8_t, sizeof(kArray)> bytes_span =
        as_bytes(make_span(kArray));
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(kArray), bytes_span.data());
    EXPECT_EQ(sizeof(kArray), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }

  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    span<const uint8_t> bytes_span = as_bytes(mutable_span);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(vec.data()), bytes_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }
}

TEST(SpanTest, AsWritableBytes) {
  std::vector<int> vec = {1, 1, 2, 3, 5, 8};
  span<int> mutable_span(vec);
  span<uint8_t> writable_bytes_span = as_writable_bytes(mutable_span);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(vec.data()), writable_bytes_span.data());
  EXPECT_EQ(sizeof(int) * vec.size(), writable_bytes_span.size());
  EXPECT_EQ(writable_bytes_span.size(), writable_bytes_span.size_bytes());

  // Set the first entry of vec to zero while writing through the span.
  std::fill(writable_bytes_span.data(),
            writable_bytes_span.data() + sizeof(int), 0);
  EXPECT_EQ(0, vec[0]);
}

TEST(SpanTest, MakeSpanFromDataAndSize) {
  int* nullint = nullptr;
  auto empty_span = make_span(nullint, 0);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};
  span<int> expected_span(vector.data(), vector.size());
  auto made_span = make_span(vector.data(), vector.size());
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromPointerPair) {
  int* nullint = nullptr;
  auto empty_span = make_span(nullint, nullint);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};
  span<int> expected_span(vector.data(), vector.size());
  auto made_span = make_span(vector.data(), vector.data() + vector.size());
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromConstexprArray) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int, 5> expected_span(kArray);
  constexpr auto made_span = make_span(kArray);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromStdArray) {
  const std::array<int, 5> kArray = {{1, 2, 3, 4, 5}};
  span<const int, 5> expected_span(kArray);
  auto made_span = make_span(kArray);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromConstContainer) {
  const std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<const int> expected_span(vector);
  auto made_span = make_span(vector);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeStaticSpanFromConstContainer) {
  const std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<const int, 5> expected_span(vector);
  auto made_span = make_span<5>(vector);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromContainer) {
  std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<int> expected_span(vector);
  auto made_span = make_span(vector);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeStaticSpanFromContainer) {
  std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<int, 5> expected_span(vector);
  auto made_span = make_span<5>(vector);
  EXPECT_EQ(expected_span.data(), make_span<5>(vector).data());
  EXPECT_EQ(expected_span.size(), make_span<5>(vector).size());
  static_assert(decltype(make_span<5>(vector))::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromDynamicSpan) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int> expected_span(kArray);
  constexpr auto made_span = make_span(expected_span);
  static_assert(std::is_same<decltype(expected_span)::element_type,
                             decltype(made_span)::element_type>::value,
                "make_span(span) should have the same element_type as span");

  static_assert(expected_span.data() == made_span.data(),
                "make_span(span) should have the same data() as span");

  static_assert(expected_span.size() == made_span.size(),
                "make_span(span) should have the same size() as span");

  static_assert(decltype(made_span)::extent == decltype(expected_span)::extent,
                "make_span(span) should have the same extent as span");

  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromStaticSpan) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int, 5> expected_span(kArray);
  constexpr auto made_span = make_span(expected_span);
  static_assert(std::is_same<decltype(expected_span)::element_type,
                             decltype(made_span)::element_type>::value,
                "make_span(span) should have the same element_type as span");

  static_assert(expected_span.data() == made_span.data(),
                "make_span(span) should have the same data() as span");

  static_assert(expected_span.size() == made_span.size(),
                "make_span(span) should have the same size() as span");

  static_assert(decltype(made_span)::extent == decltype(expected_span)::extent,
                "make_span(span) should have the same extent as span");

  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, StdTupleSize) {
  static_assert(std::tuple_size<span<int, 0>>::value == 0, "");
  static_assert(std::tuple_size<span<int, 1>>::value == 1, "");
  static_assert(std::tuple_size<span<int, 2>>::value == 2, "");
}

TEST(SpanTest, StdTupleElement) {
  static_assert(std::is_same<int, std::tuple_element_t<0, span<int, 1>>>::value,
                "");
  static_assert(
      std::is_same<const int,
                   std::tuple_element_t<0, span<const int, 2>>>::value,
      "");
  static_assert(
      std::is_same<const int*,
                   std::tuple_element_t<1, span<const int*, 2>>>::value,
      "");
}

TEST(SpanTest, StdGet) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int, 5> span(kArray);

  static_assert(
      &kArray[0] == &std::get<0>(span),
      "std::get<0>(span) does not refer to the same element as kArray[0]");
  static_assert(
      &kArray[1] == &std::get<1>(span),
      "std::get<1>(span) does not refer to the same element as kArray[1]");
  static_assert(
      &kArray[2] == &std::get<2>(span),
      "std::get<2>(span) does not refer to the same element as kArray[2]");
  static_assert(
      &kArray[3] == &std::get<3>(span),
      "std::get<3>(span) does not refer to the same element as kArray[3]");
  static_assert(
      &kArray[4] == &std::get<4>(span),
      "std::get<4>(span) does not refer to the same element as kArray[4]");
}

TEST(SpanTest, EnsureConstexprGoodness) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};
  constexpr span<const int> constexpr_span(kArray);
  const size_t size = 2;

  const size_t start = 1;
  constexpr span<const int> subspan =
      constexpr_span.subspan(start, start + size);
  for (size_t i = 0; i < subspan.size(); ++i)
    EXPECT_EQ(kArray[start + i], subspan[i]);

  constexpr span<const int> firsts = constexpr_span.first(size);
  for (size_t i = 0; i < firsts.size(); ++i)
    EXPECT_EQ(kArray[i], firsts[i]);

  constexpr span<const int> lasts = constexpr_span.last(size);
  for (size_t i = 0; i < lasts.size(); ++i) {
    const size_t j = (base::size(kArray) - size) + i;
    EXPECT_EQ(kArray[j], lasts[i]);
  }

  constexpr int item = constexpr_span[size];
  EXPECT_EQ(kArray[size], item);
}

TEST(SpanTest, OutOfBoundsDeath) {
  constexpr span<int, 0> kEmptySpan;
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan[0], "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.first(1), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.last(1), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.subspan(1), "");

  constexpr span<int> kEmptyDynamicSpan;
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan[0], "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.front(), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.first(1), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.last(1), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.back(), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.subspan(1), "");

  static constexpr int kArray[] = {0, 1, 2};
  constexpr span<const int> kNonEmptyDynamicSpan(kArray);
  EXPECT_EQ(3U, kNonEmptyDynamicSpan.size());
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan[4], "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.subspan(10), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.subspan(1, 7), "");
}

TEST(SpanTest, IteratorIsRangeMoveSafe) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  const size_t kNumElements = 5;
  constexpr span<const int> span(kArray);

  static constexpr int kOverlappingStartIndexes[] = {-4, 0, 3, 4};
  static constexpr int kNonOverlappingStartIndexes[] = {-7, -5, 5, 7};

  // Overlapping ranges.
  for (const int dest_start_index : kOverlappingStartIndexes) {
    EXPECT_FALSE(CheckedRandomAccessIterator<const int>::IsRangeMoveSafe(
        span.begin(), span.end(),
        CheckedRandomAccessIterator<const int>(
            span.data() + dest_start_index,
            span.data() + dest_start_index + kNumElements)));
    EXPECT_FALSE(CheckedRandomAccessConstIterator<const int>::IsRangeMoveSafe(
        span.cbegin(), span.cend(),
        CheckedRandomAccessConstIterator<const int>(
            span.data() + dest_start_index,
            span.data() + dest_start_index + kNumElements)));
  }

  // Non-overlapping ranges.
  for (const int dest_start_index : kNonOverlappingStartIndexes) {
    EXPECT_TRUE(CheckedRandomAccessIterator<const int>::IsRangeMoveSafe(
        span.begin(), span.end(),
        CheckedRandomAccessIterator<const int>(
            span.data() + dest_start_index,
            span.data() + dest_start_index + kNumElements)));
    EXPECT_TRUE(CheckedRandomAccessConstIterator<const int>::IsRangeMoveSafe(
        span.cbegin(), span.cend(),
        CheckedRandomAccessConstIterator<const int>(
            span.data() + dest_start_index,
            span.data() + dest_start_index + kNumElements)));
  }

  // IsRangeMoveSafe is true if the length to be moved is 0.
  EXPECT_TRUE(CheckedRandomAccessIterator<const int>::IsRangeMoveSafe(
      span.begin(), span.begin(),
      CheckedRandomAccessIterator<const int>(span.data(), span.data())));
  EXPECT_TRUE(CheckedRandomAccessConstIterator<const int>::IsRangeMoveSafe(
      span.cbegin(), span.cbegin(),
      CheckedRandomAccessConstIterator<const int>(span.data(), span.data())));

  // IsRangeMoveSafe is false if end < begin.
  EXPECT_FALSE(CheckedRandomAccessIterator<const int>::IsRangeMoveSafe(
      span.end(), span.begin(),
      CheckedRandomAccessIterator<const int>(span.data(), span.data())));
  EXPECT_FALSE(CheckedRandomAccessConstIterator<const int>::IsRangeMoveSafe(
      span.cend(), span.cbegin(),
      CheckedRandomAccessConstIterator<const int>(span.data(), span.data())));
}

}  // namespace base
