// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_FEATURE_LIST_H_
#define BASE_TEST_SCOPED_FEATURE_LIST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"

namespace base {
namespace test {

// ScopedFeatureList resets the global FeatureList instance to a new empty
// instance and restores the original instance upon destruction. When using the
// non-deprecated APIs, a corresponding FieldTrialList is also created.
//
// Note: Re-using the same object is not allowed. To reset the feature
// list and initialize it anew, destroy an existing scoped list and init
// a new one.
//
// ScopedFeatureList needs to be initialized (via one of Init*() methods)
// before running code that inspects the state of features, such as in the
// constructor of the test harness.
//
// If multiple instances of this class are used in a nested fashion, they
// should be destroyed in the opposite order of their Init*() methods being
// called.
class ScopedFeatureList final {
 public:
  ScopedFeatureList();
  ~ScopedFeatureList();

  struct FeatureAndParams {
    FeatureAndParams(const Feature& feature, const FieldTrialParams& params);
    ~FeatureAndParams();

    FeatureAndParams(const FeatureAndParams& other);

    const Feature& feature;
    const FieldTrialParams params;
  };

  // Resets the instance to a non-initialized state.
  void Reset();

  // Initializes and registers a FeatureList instance without any additional
  // enabled or disabled features. Existing state, if any, will be kept. This is
  // equivalent to calling InitWithFeatures({}, {}).
  void Init();

  // WARNING: This method will reset any globally configured features to their
  // default values, which can hide feature interaction bugs. Please use
  // sparingly.  https://crbug.com/713390
  // Initializes and registers the given FeatureList instance.
  void InitWithFeatureList(std::unique_ptr<FeatureList> feature_list);

  // WARNING: This method will reset any globally configured features to their
  // default values, which can hide feature interaction bugs. Please use
  // sparingly.  https://crbug.com/713390
  // Initializes and registers a FeatureList instance with only the given
  // enabled and disabled features (comma-separated names).
  void InitFromCommandLine(const std::string& enable_features,
                           const std::string& disable_features);

  // Initializes and registers a FeatureList instance based on present
  // FeatureList and overridden with the given enabled and disabled features.
  // Any feature overrides already present in the global FeatureList will
  // continue to apply, unless they conflict with the overrides passed into this
  // method. This is important for testing potentially unexpected feature
  // interactions.
  void InitWithFeatures(const std::vector<Feature>& enabled_features,
                        const std::vector<Feature>& disabled_features);

  // Initializes and registers a FeatureList instance based on present
  // FeatureList and overridden with single enabled feature.
  void InitAndEnableFeature(const Feature& feature);

  // Initializes and registers a FeatureList instance based on present
  // FeatureList and overridden with single enabled feature and associated field
  // trial parameters.
  // Note: this creates a scoped global field trial list if there is not
  // currently one.
  void InitAndEnableFeatureWithParameters(
      const Feature& feature,
      const FieldTrialParams& feature_parameters);

  // Initializes and registers a FeatureList instance based on present
  // FeatureList and overridden with the given enabled features and the
  // specified field trial parameters, and the given disabled features.
  // Note: This creates a scoped global field trial list if there is not
  // currently one.
  void InitWithFeaturesAndParameters(
      const std::vector<FeatureAndParams>& enabled_features,
      const std::vector<Feature>& disabled_features);

  // Initializes and registers a FeatureList instance based on present
  // FeatureList and overridden with single disabled feature.
  void InitAndDisableFeature(const Feature& feature);

  // Initializes and registers a FeatureList instance based on present
  // FeatureList and overriden with a single feature either enabled or
  // disabled depending on |enabled|.
  void InitWithFeatureState(const Feature& feature, bool enabled);

 private:
  // Initializes and registers a FeatureList instance based on present
  // FeatureList and overridden with the given enabled and disabled features.
  // Any feature overrides already present in the global FeatureList will
  // continue to apply, unless they conflict with the overrides passed into this
  // method.
  // Features to enable may be specified through either |enabled_features| or
  // |enabled_feature_and_params|, but not both (i.e. one of these must be
  // empty).
  void InitWithFeaturesImpl(
      const std::vector<Feature>& enabled_features,
      const std::vector<FeatureAndParams>& enabled_features_and_params,
      const std::vector<Feature>& disabled_features);

  // Initializes and registers a FeatureList instance based on present
  // FeatureList and overridden with single enabled feature and associated field
  // trial override.
  // |trial| is expected to outlive the ScopedFeatureList.
  void InitAndEnableFeatureWithFieldTrialOverride(const Feature& feature,
                                                  FieldTrial* trial);

  bool init_called_ = false;
  std::unique_ptr<FeatureList> original_feature_list_;
  base::FieldTrialList* original_field_trial_list_;
  std::string original_params_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFeatureList);
};

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_SCOPED_FEATURE_LIST_H_
