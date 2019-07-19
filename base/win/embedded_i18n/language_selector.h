// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a helper class for selecting a supported language from a
// set of candidates.

#ifndef BASE_WIN_EMBEDDED_I18N_LANGUAGE_SELECTOR_H_
#define BASE_WIN_EMBEDDED_I18N_LANGUAGE_SELECTOR_H_

#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"

namespace base {
namespace win {
namespace i18n {

// Selects a language from a set of available translations based on the user's
// preferred language list. An optional preferred langauge may be provided to
// override selection should a corresponding translation be available.
class BASE_EXPORT LanguageSelector {
 public:
  using LangToOffset = std::pair<base::StringPiece16, int>;

  // Constructor to be used for users of this class that will provide the actual
  // language offsets that will be used.
  // |preferred_language| is an optional language used to as higher priority
  // language when determining the matched language. This languages will
  // take precedence over the system defined languages.
  // |languages_to_offset_begin| and |languages_to_offset_end| point to a sorted
  // array of language identifiers (and their offsets) for which translations
  // are available.
  LanguageSelector(base::StringPiece16 preferred_language,
                   base::span<const LangToOffset> languages_to_offset);

  // Constructor for testing purposes.
  // |candidates| is a list of all candiate languages that can be used to
  // determine which langauge to use.
  // |languages_to_offset_begin| and |languages_to_offset_end| point to a sorted
  // array of language identifiers (and their offsets) for which translations
  // are available.
  LanguageSelector(const std::vector<base::string16>& candidates,
                   base::span<const LangToOffset> languages_to_offset);

  ~LanguageSelector();

  // The offset of the matched language (i.e., IDS_L10N_OFFSET_*).
  int offset() const { return selected_offset_; }

  // The full name of the candidate language for which a match was found.
  const base::string16& matched_candidate() const { return matched_candidate_; }

  // The name of the selected translation.
  const base::string16& selected_translation() const {
    return selected_language_;
  }

 private:
  base::string16 matched_candidate_;
  base::string16 selected_language_;
  int selected_offset_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSelector);
};

}  // namespace i18n
}  // namespace win
}  // namespace base

#endif  // BASE_WIN_EMBEDDED_I18N_LANGUAGE_SELECTOR_H_
