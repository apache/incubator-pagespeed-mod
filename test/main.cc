#include "absl/debugging/symbolize.h"
#include "test/test_runner.h"

// The main entry point (and the rest of this file) should have no logic in it,
// this allows overriding by site specific versions of main.cc.
int main(int argc, char** argv) {
#ifndef __APPLE__
  absl::InitializeSymbolizer(argv[0]);
#endif
  return PageSpeed::TestRunner::RunTests(argc, argv);
}