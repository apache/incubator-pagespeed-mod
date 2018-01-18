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


#include "pagespeed/kernel/base/statistics.h"

#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/writer.h"

namespace net_instaweb {

const char Statistics::kDefaultGroup[] = "Statistics";

namespace {
// As we do fix size buckets, each bucket has the same height.
const double kBarHeightPerBucket = 20;
// Each bucket has different width, depends on the percentage of bucket value
// out of total counts.
// The width of a bucket is percentage_of_bucket_value * kBarWidthTotal.
const double kBarWidthTotal = 400;
}  // namespace

class MessageHandler;

Variable::~Variable() {
}

UpDownCounter::~UpDownCounter() {
}

int64 UpDownCounter::SetReturningPreviousValue(int64 value) {
  int64 previous_value = Get();
  Set(value);
  return previous_value;
}

MutexedScalar::~MutexedScalar() {
}

int64 MutexedScalar::Get() const {
  if (mutex() != NULL) {
    ScopedMutex hold_lock(mutex());
    return GetLockHeld();
  } else {
    return -1;
  }
}

void MutexedScalar::Set(int64 new_value) {
  if (mutex() != NULL) {
    ScopedMutex hold_lock(mutex());
    SetLockHeld(new_value);
  }
}

int64 MutexedScalar::SetReturningPreviousValue(int64 new_value) {
  if (mutex() != NULL) {
    ScopedMutex hold_lock(mutex());
    return SetReturningPreviousValueLockHeld(new_value);
  } else {
    return -1;
  }
}

int64 MutexedScalar::AddHelper(int64 delta) {
  if (mutex() != NULL) {
    ScopedMutex hold_lock(mutex());
    return AddLockHeld(delta);
  } else {
    return -1;
  }
}

void MutexedScalar::SetLockHeld(int64 new_value) {
  SetReturningPreviousValueLockHeld(new_value);
}

int64 MutexedScalar::AddLockHeld(int64 delta) {
  int64 value = GetLockHeld() + delta;
  SetLockHeld(value);
  return value;
}

Histogram::~Histogram() {
}

CountHistogram::CountHistogram(AbstractMutex* mutex)
    : mutex_(mutex), count_(0) {}

CountHistogram::~CountHistogram() {
}

TimedVariable::~TimedVariable() {
}

FakeTimedVariable::FakeTimedVariable(StringPiece name, Statistics* stats)
    : var_(stats->AddVariable(name)) {
}

FakeTimedVariable::~FakeTimedVariable() {
}

void Histogram::WriteRawHistogramData(Writer* writer, MessageHandler* handler) {
  const char bucket_style[] = "<tr><td style=\"padding: 0 0 0 0.25em\">"
      "[</td><td style=\"text-align:right;padding:0 0.25em 0 0\">"
      "%s,</td><td style=text-align:right;padding: 0 0.25em\">%s)</td>";
  const char value_style[] = "<td style=\"text-align:right;padding:0 0.25em\">"
                             "%.f</td>";
  const char perc_style[] = "<td style=\"text-align:right;padding:0 0.25em\">"
                            "%.1f%%</td>";
  const char bar_style[] = "<td><div style=\"width: %.fpx;height:%.fpx;"
                           "background-color:blue\"></div></td>";
  double count = CountInternal();
  double perc = 0;
  double cumulative_perc = 0;
  // Write prefix of the table.
  writer->Write("<table>", handler);
  for (int i = 0, n = NumBuckets(); i < n; ++i) {
    double value = BucketCount(i);
    if (value == 0) {
      // We do not draw empty bucket.
      continue;
    }
    double lower_bound = BucketStart(i);
    double upper_bound = BucketLimit(i);

    GoogleString lower_bound_string = StringPrintf("%.0f", lower_bound);
    if (lower_bound == -std::numeric_limits<double>::infinity()) {
      lower_bound_string = "-&infin;";
    }
    GoogleString upper_bound_string = StringPrintf("%.0f", upper_bound);
    if (upper_bound == std::numeric_limits<double>::infinity()) {
      upper_bound_string = "&infin;";
    }

    perc = value * 100 / count;
    cumulative_perc += perc;
    GoogleString output = StrCat(
        StringPrintf(bucket_style, lower_bound_string.c_str(),
                     upper_bound_string.c_str()),
        StringPrintf(value_style, value),
        StringPrintf(perc_style, perc),
        StringPrintf(perc_style, cumulative_perc),
        StringPrintf(bar_style, (perc * kBarWidthTotal) / 100,
                     kBarHeightPerBucket));
    writer->Write(output, handler);
  }
  // Write suffix of the table.
  writer->Write("</table>", handler);
}

void Histogram::Render(int index, Writer* writer, MessageHandler* handler) {
  writer->Write(StringPrintf("<div id='hist_%d' style='display:none'>", index),
                handler);

  // Don't hold a lock while calling the writer, as this can deadlock if
  // the writer itself winds up invoking pagespeed, causing the histogram
  // to be locked.  So buffer each histogram and release the lock before
  // passing it to writer.
  GoogleString buf;
  {
    ScopedMutex hold(lock());
    StringWriter string_writer(&buf);
    WriteRawHistogramData(&string_writer, handler);
  }

  writer->Write(buf, handler);
  writer->Write("</div>\n", handler);
}

Statistics::~Statistics() {
}

UpDownCounter* Statistics::AddGlobalUpDownCounter(const StringPiece& name) {
  return AddUpDownCounter(name);
}

namespace {

const char kHistogramProlog[] =
    "<div>\n"
    "  <table>\n"
    "    <thead><tr>\n"
    "      <td>Histogram Name (click to view)</td>\n"
    "      <td>Count</td>\n"
    "      <td>Avg</td>\n"
    "      <td>StdDev</td>\n"
    "      <td>Min</td>\n"
    "      <td>Median</td>\n"
    "      <td>Max</td>\n"
    "      <td>90%</td>\n"
    "      <td>95%</td>\n"
    "      <td>99%</td>\n"
    "    </tr></thead><tbody>\n";

const char kHistogramRowFormat[] =
    "      <tr id='hist_row_%d'>\n"
    "        <td><label><input type='radio' name='choose_histogram'%s\n"
    "                   onchange='setHistogram(%d)'>%s</label></td>\n"
    "        <td>%.0f</td><td>%.1f</td><td>%.1f</td>\n"  // count, avg, stddev
    "        <td>%.0f</td><td>%.0f</td><td>%.0f</td>\n"  // min, median, max
    "        <td>%.0f</td><td>%.0f</td><td>%.0f</td>\n"  // 90%, 95%, 99%
    "     </tr>\n";

const char kHistogramEpilog[] =
    "    </tbody>\n"
    "  </table>\n"
    "</div>\n";

const char kHistogramScript[] =
    "<script>\n"
    "  var currentHistogram = -1;\n"
    "  function setHistogram(id) {\n"
    "    var div = document.getElementById('hist_' + currentHistogram);\n"
    "    if (div) {\n"
    "      div.style.display = 'none';\n"
    "    }\n"
    "    div = document.getElementById('hist_' + id);\n"
    "    if (div) {\n"
    "      div.style.display = '';\n"
    "    }\n"
    "    var row = document.getElementById('hist_row_' + currentHistogram);\n"
    "    if (row) {\n"
    "      row.style.backgroundColor = 'white';\n"
    "    }\n"
    "    row = document.getElementById('hist_row_' + id);\n"
    "    if (row) {\n"
    "      row.style.backgroundColor = 'yellow';\n"
    "    }\n"
    "    currentHistogram = id;\n"
    "  }\n"
    "  setHistogram(0);\n"
    "</script>\n";

}  // namespace

void Statistics::RenderHistograms(Writer* writer, MessageHandler* handler) {
  StringVector hist_names = HistogramNames();  // includes empty ones.
  StringVector populated_histogram_names;
  std::vector<Histogram*> populated_histograms;

  // Find non-empty histograms.  Note that when the server first comes
  // up, there won't be any data in the histograms because there is no
  // traffic.  Other histograms may never be populated depending on
  // mod_pagespeed settings.  We pre-scan the histograms, capturing a
  // snapshot of the non-empty ones, because a histogram might become
  // non-empty asynchronously between the next two loops, and that
  // would skew indexing.
  for (int i = 0, n = hist_names.size(); i < n; ++i) {
    Histogram* hist = FindHistogram(hist_names[i]);

    // Exclude histograms with zero count.
    if (hist->Count() != 0) {
      populated_histograms.push_back(hist);
      populated_histogram_names.push_back(hist_names[i]);
    }
  }

  writer->Write("<hr/>", handler);

  // Write table data for each histogram.
  if (populated_histograms.empty()) {
    writer->Write("<em>No histogram data yet.  Refresh once there is "
                  "traffic.</em>", handler);
  } else {
    // Write the table header for all histograms.
    writer->Write(StringPiece(kHistogramProlog,
                              STATIC_STRLEN(kHistogramProlog)),
                  handler);

    // Write a row of the table data for each non-empty histogram.
    CHECK_EQ(populated_histogram_names.size(), populated_histograms.size());
    for (int i = 0, n = populated_histograms.size(); i < n; ++i) {
      Histogram* hist = populated_histograms[i];
      GoogleString row = hist->HtmlTableRow(populated_histogram_names[i], i);
      writer->Write(row, handler);
    }
    writer->Write(StringPiece(kHistogramEpilog,
                              STATIC_STRLEN(kHistogramEpilog)),
                  handler);

    // Render the non-empty histograms.
    for (int i = 0, n = populated_histograms.size(); i < n; ++i) {
      populated_histograms[i]->Render(i, writer, handler);
    }

    // Write the JavaScript to display the histograms and highlight the row
    // when selected.
    writer->Write(StringPiece(kHistogramScript,
                              STATIC_STRLEN(kHistogramScript)),
                  handler);
  }
  writer->Write("<hr/>\n", handler);
}

GoogleString Histogram::HtmlTableRow(const GoogleString& title, int index) {
  ScopedMutex hold(lock());
  return StringPrintf(
      kHistogramRowFormat,
      index,
      (index == 0) ? " selected" : "",
      index,
      title.c_str(),
      CountInternal(),
      AverageInternal(),
      StandardDeviationInternal(),
      MinimumInternal(),
      PercentileInternal(50),
      MaximumInternal(),
      PercentileInternal(90),
      PercentileInternal(95),
      PercentileInternal(99));
}

void Statistics::RenderTimedVariables(Writer* writer,
                                      MessageHandler* message_handler) {
  TimedVariable* timedvar = NULL;
  const GoogleString end("</table>\n<td>\n<td>\n");
  std::map<GoogleString, StringVector> group_map = TimedVariableMap();
  std::map<GoogleString, StringVector>::const_iterator p;
  // Export statistics in each group in one table.
  for (p = group_map.begin(); p != group_map.end(); ++p) {
    // Write table header for each group.
    const GoogleString begin = StrCat(
        "<p><table bgcolor=#eeeeff width=100%%>",
        "<tr align=center><td><font size=+2>", p->first,
        "</font></td></tr></table>",
        "</p>\n<td>\n<td>\n<td>\n<td>\n<td>\n",
        "<table bgcolor=#fff5ee frame=box cellspacing=1 cellpadding=2>\n",
        "<tr bgcolor=#eee5de><td>"
        "<form action=\"/statusz/reset\" method = \"post\">"
        "<input type=\"submit\" value = \"Reset Statistics\"></form></td>"
        "<th align=right>TenSec</th><th align=right>Minute</th>"
        "<th align=right>Hour</th><th align=right>Total</th></tr>");
    writer->Write(begin, message_handler);
    // Write each statistic as a row in the table.
    for (int i = 0, n = p->second.size(); i < n; ++i) {
      timedvar = FindTimedVariable(p->second[i]);
      const GoogleString content = StringPrintf("<tr><td> %s </td>"
          "<td align=right> %s </td><td align=right> %s </td>"
          "<td align=right> %s </td><td align=right> %s </td></tr>",
      p->second[i].c_str(),
      Integer64ToString(timedvar->Get(TimedVariable::TENSEC)).c_str(),
      Integer64ToString(timedvar->Get(TimedVariable::MINUTE)).c_str(),
      Integer64ToString(timedvar->Get(TimedVariable::HOUR)).c_str(),
      Integer64ToString(timedvar->Get(TimedVariable::START)).c_str());
      writer->Write(content, message_handler);
    }
    // Write table ending part.
    writer->Write(end, message_handler);
  }
}

int64 Statistics::LookupValue(StringPiece stat_name) {
  Variable* var = FindVariable(stat_name);
  if (var != NULL) {
    return var->Get();
  }
  UpDownCounter* counter = FindUpDownCounter(stat_name);
  if (counter != NULL) {
    return counter->Get();
  }
  TimedVariable* tvar = FindTimedVariable(stat_name);
  if (tvar != NULL) {
    return tvar->Get(TimedVariable::START);
  }
  LOG(FATAL) << "Could not find stat: " << stat_name;
  return 0;
}

}  // namespace net_instaweb
