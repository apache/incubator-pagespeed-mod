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

#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_STATISTICS_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_STATISTICS_H_

#include <cstddef>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/statistics_template.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class FileSystem;
class MessageHandler;
class StatisticsLogger;
class Timer;

// An implementation of Statistics using our shared memory infrastructure.
// These statistics will be shared amongst all processes and threads
// spawned by our host.  Note that we will be obtaining a per-variable mutex for
// every read and write to these variables.  Since this may be expensive,
// we may need each thread to keep a local cache and infrequently write
// through to this Statistics object.  TODO(abliss): actually do this.
//
// Because we must allocate shared memory segments and mutexes before any child
// processes and threads are created, all AddVariable calls must be done in
// the host before it starts forking/threading. Once all variables are added,
// you must call InitVariables(true), and then InitVariables(false) in every
// kid.
//
// If a variable fails to initialize (due to either its mutex or the shared
// memory segment not working), it will not increment in that process (and a
// warning message will be logged).  If the variable fails to initialize in the
// process that happens to serve a statistics page, then the variable will show
// up with value -1.
class SharedMemVariable : public MutexedScalar {
 public:
  SharedMemVariable(StringPiece name, Statistics* stats);
  ~SharedMemVariable() override {}
  virtual StringPiece GetName() const { return name_; }

 protected:
  AbstractMutex* mutex() const override;
  int64 GetLockHeld() const override;
  int64 SetReturningPreviousValueLockHeld(int64 value) override;

 private:
  friend class SharedMemStatistics;
  friend class SharedMemTimedVariable;

  explicit SharedMemVariable(const StringPiece& name);

  void AttachTo(AbstractSharedMemSegment* segment_, size_t offset,
                MessageHandler* message_handler);

  // Called on initialization failure, to make sure it's clear if we
  // share some state with parent.
  void Reset();

  // The name of this variable.
  const GoogleString name_;

  // Lock protecting us. NULL if for some reason initialization failed.
  std::unique_ptr<AbstractMutex> mutex_;

  // The data...
  volatile int64* value_ptr_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemVariable);
};

class SharedMemHistogram : public Histogram {
 public:
  SharedMemHistogram(StringPiece name, Statistics* stats);

  ~SharedMemHistogram() override;
  void Add(double value) override;
  void Clear() override;
  int NumBuckets() override;
  // Call the following functions after statistics->Init and before add values.
  // EnableNegativeBuckets, SetMinValue and SetMaxValue will
  // cause resetting Histogram.
  void EnableNegativeBuckets() override;
  // Set the minimum value allowed in histogram.
  void SetMinValue(double value) override;
  // Set the upper-bound of value in histogram,
  // The value range in histogram is [MinValue, MaxValue) or
  // (-MaxValue, MaxValue) if negative buckets are enabled.
  void SetMaxValue(double value) override;

  // We rely on NumBuckets to allocate a memory segment for the histogram, so
  // this should be called right after AddHistogram() in the ::Initialize
  // process. Similarly, all the bounds must be initialized at that point, to
  // avoid clearing the histogram as new child processes attach to it.
  void SetSuggestedNumBuckets(int i) override;

  // Return the amount of shared memory this Histogram objects needs for its
  // use.
  size_t AllocationSize(AbstractSharedMem* shm_runtime) {
    // Shared memory space should include a mutex, HistogramBody and the storage
    // for the actual buckets.
    return shm_runtime->SharedMutexSize() + sizeof(HistogramBody) +
           sizeof(double) * NumBuckets();
  }

 protected:
  AbstractMutex* lock() override { return mutex_.get(); }
  double AverageInternal() override;
  double PercentileInternal(const double perc) override;
  double StandardDeviationInternal() override;
  double CountInternal() override;
  double MaximumInternal() override;
  double MinimumInternal() override;
  double BucketStart(int index) override;
  double BucketCount(int index) override;

 private:
  friend class SharedMemStatistics;
  void AttachTo(AbstractSharedMemSegment* segment, size_t offset,
                MessageHandler* message_handler);

  // Returns the width of normal buckets (as in not the two extreme outermost
  // buckets which have infinite width).
  double BucketWidth();

  // Finds a bucket that should contain the given value. Note that this does
  // not consider the catcher buckets for out-of-range values.
  int FindBucket(double value);
  void Init();
  void DCheckRanges() const;
  void Reset();
  void ClearInternal();  // expects mutex_ held, buffer_ != NULL
  const GoogleString name_;
  std::unique_ptr<AbstractMutex> mutex_;
  // TODO(fangfei): implement a non-shared-mem histogram.
  struct HistogramBody {
    // Enable negative values in histogram, false by default.
    bool enable_negative_;
    // Minimum value allowed in Histogram, 0 by default.
    double min_value_;
    // Maximum value allowed in Histogram,
    // numeric_limits<double>::max() by default.
    double max_value_;
    // Real minimum value.
    double min_;
    // Real maximum value.
    double max_;
    double count_;
    double sum_;
    double sum_of_squares_;
    // Histogram buckets data.
    double values_[1];
  };
  // Number of buckets in this histogram.
  int num_buckets_;
  HistogramBody* buffer_;  // may be NULL if init failed.
  DISALLOW_COPY_AND_ASSIGN(SharedMemHistogram);
};

class SharedMemStatistics
    : public ScalarStatisticsTemplate<SharedMemVariable, SharedMemHistogram,
                                      FakeTimedVariable> {
 public:
  SharedMemStatistics(int64 logging_interval_ms, int64 max_logfile_size_kb,
                      const StringPiece& logging_file, bool logging,
                      const GoogleString& filename_prefix,
                      AbstractSharedMem* shm_runtime,
                      MessageHandler* message_handler, FileSystem* file_system,
                      Timer* timer);
  ~SharedMemStatistics() override;

  // This method initializes or attaches to shared memory. You should call this
  // exactly once in each process/thread, after all calls to AddVariables,
  // AddHistograms and SetSuggestedNumBuckets (as well as any other histogram
  // range configurations) have been done.
  //
  // The root process (the one that starts all the other child
  // threads and processes) must be the first one to make the call, with
  // parent = true, with all other calling it with = false.
  //
  // Returns true if successful.
  bool Init(bool parent, MessageHandler* message_handler);

  // This should be called from the root process as it is about to exit, when
  // no further children are expected to start.
  void GlobalCleanup(MessageHandler* message_handler);

  // Like above, but can be done after object got cleaned up, by passing
  // a saved SegmentName(). Precondition: init must have returned 'true'.
  static void GlobalCleanup(AbstractSharedMem* shm_runtime,
                            const GoogleString& segment_name,
                            MessageHandler* message_handler);

  GoogleString SegmentName() const;

  // TODO(sligocki): Rename to statistics_logger().
  StatisticsLogger* console_logger() override { return console_logger_.get(); }

 protected:
  Var* NewVariable(StringPiece name) override;
  UpDown* NewUpDownCounter(StringPiece name) override;
  Hist* NewHistogram(StringPiece name) override;

 private:
  // Create mutexes in the segment, with per_var bytes being used,
  // counting the mutex, for each variable.
  bool InitMutexes(size_t per_var, MessageHandler* message_handler);

  friend class SharedMemStatisticsTestBase;

  AbstractSharedMem* shm_runtime_;
  GoogleString filename_prefix_;
  std::unique_ptr<AbstractSharedMemSegment> segment_;
  bool frozen_;
  // TODO(sligocki): Rename.
  std::unique_ptr<StatisticsLogger> console_logger_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemStatistics);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_STATISTICS_H_
