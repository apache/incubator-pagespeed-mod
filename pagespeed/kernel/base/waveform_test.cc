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


#include "pagespeed/kernel/base/waveform.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/statistics_template.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace {

const char kCoordinateFormat[] = "[%f, %f]";

}  // namespace

namespace net_instaweb {

class WaveformTest : public testing::Test {
 protected:
  WaveformTest()
      : thread_system_(Platform::CreateThreadSystem()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
        stats_(thread_system_.get()),
        handler_(thread_system_->NewMutex()) {
  }

  GoogleString Format(int time_ms, int value) {
    return StringPrintf(kCoordinateFormat, 1.0 * time_ms, 1.0 * value);
  }

  bool Contains(const StringPiece& html, int time_ms, int value) {
    return (html.find(Format(time_ms, value)) != GoogleString::npos);
  }

  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  SimpleStats stats_;
  MockMessageHandler handler_;
};


// A basic sanity test showing that the header loads the jsapi.
TEST_F(WaveformTest, Header) {
  GoogleString html;
  StringWriter writer(&html);
  Waveform::RenderHeader(&writer, &handler_);
  EXPECT_NE(GoogleString::npos, html.find("www.google.com/jsapi"));
}

// Instantiate a waveform and make sure one of the values shows up.
TEST_F(WaveformTest, BasicGraph) {
  UpDownCounter* variable = stats_.AddUpDownCounter("test1");

  Waveform waveform(thread_system_.get(), &timer_, 10, variable);
  timer_.SetTimeUs(MockTimer::kApr_5_2010_ms);
  waveform.Add(10);
  timer_.AdvanceMs(10);
  waveform.Add(20);
  timer_.AdvanceMs(10);
  waveform.Add(10);
  timer_.AdvanceMs(10);
  waveform.Add(30);
  timer_.AdvanceMs(10);
  waveform.Add(10);
  timer_.AdvanceMs(10);
  waveform.Add(40);
  timer_.AdvanceMs(10);
  waveform.Add(10);
  timer_.AdvanceMs(10);
  waveform.Add(50);
  timer_.AdvanceMs(10);
  waveform.Add(10);
  timer_.AdvanceMs(10);
  waveform.Add(60);
  timer_.AdvanceMs(10);

  GoogleString html;
  StringWriter writer(&html);
  waveform.Render("My Waveform", "My Values", &writer, &handler_);
  EXPECT_TRUE(Contains(html, 90, 60));
  EXPECT_NE(GoogleString::npos, html.find("'My Waveform'"));
  EXPECT_NE(GoogleString::npos, html.find("'My Values'"));
  // The last Add call is sticky.
  EXPECT_EQ(60, variable->Get());
}

// Instantiate a waveform and make sure one of the values shows up.
TEST_F(WaveformTest, Delta) {
  UpDownCounter* variable = stats_.AddUpDownCounter("test1");
  Waveform waveform(thread_system_.get(), &timer_, 10, variable);
  timer_.SetTimeUs(MockTimer::kApr_5_2010_ms);
  waveform.AddDelta(10);
  timer_.AdvanceMs(10);
  waveform.AddDelta(20);
  timer_.AdvanceMs(10);
  waveform.AddDelta(-5);
  timer_.AdvanceMs(10);

  GoogleString html;
  StringWriter writer(&html);
  waveform.Render("My Waveform", "My Values", &writer, &handler_);
  EXPECT_TRUE(Contains(html, 20, 25));
  EXPECT_NE(GoogleString::npos, html.find("'My Waveform'"));
  EXPECT_NE(GoogleString::npos, html.find("'My Values'"));
  EXPECT_EQ(10 + 20 - 5, variable->Get());
}

// Overflows the number of samples and makes sure the desired results
// are shown.
TEST_F(WaveformTest, Overflow) {
  Waveform waveform(thread_system_.get(), &timer_, 10, NULL /* variable */);

  // Don't overflow at first.
  for (int i = 0; i < 10; ++i) {
    waveform.Add(i);
    timer_.AdvanceMs(10);
  }
  GoogleString html;
  StringWriter writer(&html);

  waveform.Render("My Waveform", "My Values", &writer, &handler_);
  EXPECT_TRUE(Contains(html, 0, 0));
  EXPECT_TRUE(Contains(html, 10, 1));
  EXPECT_TRUE(Contains(html, 80, 8));
  EXPECT_TRUE(Contains(html, 90, 9));

  // Now overflow the first 2 entries and re-render.  The first &
  // second values (0,0) and (10,1) should be gone, but note that
  // the time-axis is all deltas so the x-values always start at 0.
  for (int i = 10; i < 12; ++i) {  // knocks off first 2
    waveform.Add(i);
    timer_.AdvanceMs(10);
  }

  html.clear();
  waveform.Render("My Waveform", "My Values", &writer, &handler_);
  EXPECT_FALSE(Contains(html, 0, 1));   // truncated
  EXPECT_FALSE(Contains(html, 10, 1));  // truncated
  EXPECT_TRUE(Contains(html, 0, 2));
  EXPECT_TRUE(Contains(html, 80, 10));
  EXPECT_TRUE(Contains(html, 90, 11));

  // The rest of the values should be present in the HTML and in order.
  GoogleString::size_type prev_pos = 0;
  for (int i = 0; i < 10; ++i) {
    GoogleString::size_type pos = html.find(Format(10 * i, i + 2));
    ASSERT_NE(GoogleString::npos, pos);
    EXPECT_LT(prev_pos, pos);
    prev_pos = pos;
  }
}

TEST_F(WaveformTest, AvgMinMax) {
  Waveform waveform(thread_system_.get(), &timer_, 10, NULL /* variable */);
  for (int i = 1; i <= 1000; ++i) {
    waveform.Add(i);
    timer_.AdvanceMs(10);
  }

  // Note that the first value involved in the average is 0 due to the
  // fact that we are accumulating delta_time*value quantities.
  EXPECT_EQ(500.0, waveform.Average());
  EXPECT_EQ(1.0, waveform.Minimum());
  EXPECT_EQ(1000.0, waveform.Maximum());
}

}  // namespace net_instaweb
