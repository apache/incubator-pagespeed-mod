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
// Tests a few different alternatives to implementing an ordered container
// supporting push_back and pop_front.
//
//
// Benchmark                   Time(ns)    CPU(ns) Iterations
// ----------------------------------------------------------
// BM_List4                        4846       4830     142857
// BM_Deque4                        747        750    1000000
// BM_VectorDeque4                  468        470    1489362
// BM_DequeUsingStdVector4         1874       1873     368421
// BM_List100                    118003     118292       5833
// BM_Deque100                    16389      16457      43750
// BM_VectorDeque100              10296      10214      63636
// BM_DequeUsingStdVector100      75617      74286       8750
//
// Disclaimer: comparing runs over time and across different machines
// can be misleading.  When contemplating an algorithm change, always do
// interleaved runs with the old & new algorithm.

#include <deque>
#include <list>
#include <vector>

#include "base/logging.h"
#include "benchmark/benchmark.h"
#include "pagespeed/kernel/base/vector_deque.h"

// Implementation of deque subset interface using vector, with O(N)
// mutations at front and no extra memory.  This is for benchmarking
// comparison.  Surprisingly it beats List even @ 100 elements.
template <class T>
class DequeUsingStdVector : public std::vector<T> {
 public:
  void push_front(const T& value) { this->insert(this->begin(), value); }
  void pop_front() { this->erase(this->begin()); }
};

template <class Deque>
static void FourElementWorkout(benchmark::State& state, int num_elements) {
  for (int iter = 0; iter < state.iterations(); ++iter) {
    Deque deque;

    // Simple usage as pure stack or queue, but not at the same time.
    for (int i = 0; i < num_elements; ++i) {
      deque.push_back(i);
    }
    for (int i = 0; i < num_elements; ++i) {
      CHECK_EQ(i, deque.front());
      deque.pop_front();
    }
    for (int i = 0; i < num_elements; ++i) {
      deque.push_front(i);
    }
    for (int i = num_elements - 1; i >= 0; --i) {
      CHECK_EQ(i, deque.front());
      deque.pop_front();
    }
    for (int i = 0; i < num_elements; ++i) {
      deque.push_front(i);
    }
    for (int i = 0; i < num_elements; ++i) {
      CHECK_EQ(i, deque.back());
      deque.pop_back();
    }
    for (int i = 0; i < num_elements; ++i) {
      deque.push_back(i);
    }
    for (int i = num_elements - 1; i >= 0; --i) {
      CHECK_EQ(i, deque.back());
      deque.pop_back();
    }

    // Comingled pushes to front or back of queue.
    for (int i = 0; i < num_elements / 2; ++i) {
      deque.push_back(i);
      deque.push_front(i);
    }
    for (int i = 0; i < num_elements; ++i) {
      deque.pop_back();
    }
    for (int i = 0; i < num_elements / 2; ++i) {
      deque.push_back(i);
      deque.push_front(i);
    }
    for (int i = 0; i < num_elements; ++i) {
      deque.pop_front();
    }
    for (int i = 0; i < num_elements / 2; ++i) {
      deque.push_front(i);
      deque.push_back(i);
    }
    for (int i = 0; i < num_elements; ++i) {
      deque.pop_back();
    }
    for (int i = 0; i < num_elements / 2; ++i) {
      deque.push_front(i);
      deque.push_back(i);
    }
    for (int i = 0; i < num_elements; ++i) {
      deque.pop_front();
    }

    // Chasing 1 value pushed onto the back and popped from front.
    for (int i = 0; i < 10 * num_elements; ++i) {
      deque.push_back(i);
      CHECK_EQ(i, deque.front());
      deque.pop_front();
    }

    // Chasing 2 values pushed onto the back and popped from front.
    deque.push_back(-1);
    for (int i = 0; i < 10 * num_elements; ++i) {
      deque.push_back(i);
      CHECK_EQ(i - 1, deque.front());
      deque.pop_front();
    }
    deque.pop_front();

    // Chasing 1 value pushed onto the front and popped from back.
    for (int i = 0; i < 10 * num_elements; ++i) {
      deque.push_front(i);
      CHECK_EQ(i, deque.back());
      deque.pop_back();
    }

    // Chasing 2 values pushed onto the front and popped from back.
    deque.push_front(-1);
    for (int i = 0; i < 10 * num_elements; ++i) {
      deque.push_front(i);
      CHECK_EQ(i - 1, deque.back());
      deque.pop_back();
    }
    deque.pop_back();
  }
}

static void BM_List4(benchmark::State& state) {
  FourElementWorkout<std::list<int> >(state, 4);
}

static void BM_Deque4(benchmark::State& state) {
  FourElementWorkout<std::deque<int> >(state, 4);
}

static void BM_VectorDeque4(benchmark::State& state) {
  FourElementWorkout<net_instaweb::VectorDeque<int> >(state, 4);
}

static void BM_DequeUsingStdVector4(benchmark::State& state) {
  FourElementWorkout<DequeUsingStdVector<int> >(state, 4);
}

static void BM_List100(benchmark::State& state) {
  FourElementWorkout<std::list<int> >(state, 100);
}

static void BM_Deque100(benchmark::State& state) {
  FourElementWorkout<std::deque<int> >(state, 100);
}

static void BM_VectorDeque100(benchmark::State& state) {
  FourElementWorkout<net_instaweb::VectorDeque<int> >(state, 100);
}

static void BM_DequeUsingStdVector100(benchmark::State& state) {
  FourElementWorkout<DequeUsingStdVector<int> >(state, 100);
}

BENCHMARK(BM_List4);
BENCHMARK(BM_Deque4);
BENCHMARK(BM_VectorDeque4);
BENCHMARK(BM_DequeUsingStdVector4);

BENCHMARK(BM_List100);
BENCHMARK(BM_Deque100);
BENCHMARK(BM_VectorDeque100);
BENCHMARK(BM_DequeUsingStdVector100);
