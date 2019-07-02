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


#include "net/instaweb/rewriter/public/css_minify.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/writer.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/identifier.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/selector.h"
#include "webutil/css/tostring.h"
#include "webutil/css/value.h"
#include "webutil/html/htmlcolor.h"

namespace net_instaweb {

bool CssMinify::Stylesheet(const Css::Stylesheet& stylesheet,
                           Writer* writer,
                           MessageHandler* handler) {
  // Get an object to encapsulate writing.
  CssMinify minifier(writer, handler);
  minifier.Minify(stylesheet);
  return minifier.ok_;
}

bool CssMinify::ParseStylesheet(StringPiece stylesheet_text) {
  ok_ = true;
  Css::Parser parser(stylesheet_text);
  parser.set_preservation_mode(true);  // Leave in unparseable regions.
  parser.set_quirks_mode(false);  // Don't fix badly formatted colors.
  scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());

  // Report error summary.
  if (error_writer_ != NULL) {
    if (parser.errors_seen_mask() != Css::Parser::kNoError) {
      error_writer_->Write(StringPrintf(
          "CSS parsing error mask %s\n",
          Integer64ToString(parser.errors_seen_mask()).c_str()), handler_);
    }
    if (parser.unparseable_sections_seen_mask() != Css::Parser::kNoError) {
      error_writer_->Write(StringPrintf(
          "CSS unparseable sections mask %s\n",
          Integer64ToString(parser.unparseable_sections_seen_mask()).c_str()),
                        handler_);
    }
    // Report individual errors.
    for (int i = 0, n = parser.errors_seen().size(); i < n; ++i) {
      Css::Parser::ErrorInfo error = parser.errors_seen()[i];
      error_writer_->Write(error.message, handler_);
      error_writer_->Write("\n", handler_);
    }
  }

  Minify(*stylesheet);
  return ok_ && (parser.errors_seen_mask() == Css::Parser::kNoError);
}

bool CssMinify::Declarations(const Css::Declarations& declarations,
                             Writer* writer,
                             MessageHandler* handler) {
  // Get an object to encapsulate writing.
  CssMinify minifier(writer, handler);
  minifier.JoinMinify(declarations, ";");
  return minifier.ok_;
}

CssMinify::CssMinify(Writer* writer, MessageHandler* handler)
    : writer_(writer), error_writer_(NULL), handler_(handler), ok_(true),
      url_collector_(NULL), in_css_calc_function_(false) {
}

CssMinify::~CssMinify() {
}

// Write if we have not encountered write error yet.
void CssMinify::Write(const StringPiece& str) {
  if (ok_) {
    ok_ &= writer_->Write(str, handler_);
  }
}

void CssMinify::WriteURL(const UnicodeText& url) {
  StringPiece string_url(url.utf8_data(), url.utf8_length());
  if (url_collector_ != NULL) {
    string_url.CopyToString(StringVectorAdd(url_collector_));
  }
  Write(Css::EscapeUrl(string_url));
}

// Write out minified version of each element of vector using supplied function
// separated by sep.
template<typename Container>
void CssMinify::JoinMinify(const Container& container, const StringPiece& sep) {
  JoinMinifyIter(container.begin(), container.end(), sep);
}

template<typename Iterator>
void CssMinify::JoinMinifyIter(const Iterator& begin, const Iterator& end,
                               const StringPiece& sep) {
  for (Iterator iter = begin; iter != end; ++iter) {
    if (iter != begin) {
      Write(sep);
    }
    Minify(**iter);
  }
}

template<>
void CssMinify::JoinMinifyIter<Css::FontFaces::const_iterator>(
    const Css::FontFaces::const_iterator& begin,
    const Css::FontFaces::const_iterator& end,
    const StringPiece& sep) {
  // Go through the list of @font-faces finding the contiguous subsets with the
  // same set of media (f.ex [a b b b a a] -> [a] [b b b] [a a]). For each
  // such subset, emit the start of the @media rule (if required), then emit
  // each @font-face without an @media rule, separating them by the given 'sep',
  // then emit the end of the @media rule (if required).
  for (Css::FontFaces::const_iterator iter = begin; iter != end; ) {
    const Css::MediaQueries& first_media_queries = (*iter)->media_queries();
    MinifyMediaStart(first_media_queries);
    MinifyFontFaceIgnoringMedia(**iter);
    for (++iter; iter != end && Equals(first_media_queries,
                                       (*iter)->media_queries()); ++iter) {
      Write(sep);
      MinifyFontFaceIgnoringMedia(**iter);
    }
    MinifyMediaEnd(first_media_queries);
  }
}

template<>
void CssMinify::JoinMinifyIter<Css::Rulesets::const_iterator>(
    const Css::Rulesets::const_iterator& begin,
    const Css::Rulesets::const_iterator& end,
    const StringPiece& sep) {
  // Go through the list of rulesets finding the contiguous subsets with the
  // same set of media (f.ex [a b b b a a] -> [a] [b b b] [a a]). For each
  // such subset, emit the start of the @media rule (if required), then emit
  // each ruleset without an @media rule, separating them by the given 'sep',
  // then emit the end of the @media rule (if required).
  for (Css::Rulesets::const_iterator iter = begin; iter != end; ) {
    const Css::MediaQueries& first_media_queries = (*iter)->media_queries();
    MinifyMediaStart(first_media_queries);
    MinifyRulesetIgnoringMedia(**iter);
    for (++iter; iter != end && Equals(first_media_queries,
                                       (*iter)->media_queries()); ++iter) {
      Write(sep);
      MinifyRulesetIgnoringMedia(**iter);
    }
    MinifyMediaEnd(first_media_queries);
  }
}


// Write the minified versions of each type. Most of these are called via
// templated instantiations of JoinMinify (or JoinMinifyIter) so that we can
// abstract the idea of minifying all sub-elements of a vector and joining them
// together.
//   Adapted from webutil/css/tostring.cc

void CssMinify::Minify(const Css::Stylesheet& stylesheet) {
  // We might want to add in unnecessary newlines between rules and imports
  // so that some readability is preserved.
  Minify(stylesheet.charsets());
  JoinMinify(stylesheet.imports(), "");
  // Note: Adjacent @font-face with the same media type are placed in the same
  // @media block. The same is true for adjacent Ruelsets. However, we do not
  // yet combine @font-face with Rulesets into the same @media block because
  // we do not expect this to be worth the trouble.
  JoinMinify(stylesheet.font_faces(), "");
  JoinMinify(stylesheet.rulesets(), "");
}

void CssMinify::Minify(const Css::Charsets& charsets) {
  for (Css::Charsets::const_iterator iter = charsets.begin();
       iter != charsets.end(); ++iter) {
    Write("@charset \"");
    Write(Css::EscapeString(*iter));
    Write("\";");
  }
}

void CssMinify::Minify(const Css::Import& import) {
  Write("@import url(");
  WriteURL(import.link());
  Write(")");
  if (!import.media_queries().empty()) {
    Write(" ");
    JoinMinify(import.media_queries(), ",");
  }
  Write(";");
}

void CssMinify::Minify(const Css::MediaQuery& media_query) {
  switch (media_query.qualifier()) {
    case Css::MediaQuery::ONLY:
      Write("only ");
      break;
    case Css::MediaQuery::NOT:
      Write("not ");
      break;
    case Css::MediaQuery::NO_QUALIFIER:
      break;
  }

  Write(Css::EscapeIdentifier(media_query.media_type()));
  if (!media_query.media_type().empty() && !media_query.expressions().empty()) {
    Write(" and ");
  }
  JoinMinify(media_query.expressions(), " and ");
}

void CssMinify::Minify(const Css::MediaExpression& expression) {
  Write("(");
  Write(Css::EscapeIdentifier(expression.name()));
  if (expression.has_value()) {
    Write(":");
    const UnicodeText& value = expression.value();
    // Note: Value is an unparsed region of raw bytes. So don't escape it.
    Write(StringPiece(value.utf8_data(), value.utf8_length()));
  }
  Write(")");
}

void CssMinify::MinifyMediaStart(const Css::MediaQueries& media_queries) {
  if (!media_queries.empty()) {
    Write("@media ");
    JoinMinify(media_queries, ",");
    Write("{");
  }
}

void CssMinify::MinifyMediaEnd(const Css::MediaQueries& media_queries) {
  if (!media_queries.empty()) {
    Write("}");
  }
}

void CssMinify::MinifyFontFaceIgnoringMedia(const Css::FontFace& font_face) {
  Write("@font-face{");
  JoinMinify(font_face.declarations(), ";");
  Write("}");
}

void CssMinify::MinifyRulesetIgnoringMedia(const Css::Ruleset& ruleset) {
  // TODO(sligocki): Only write out ruleset if declarations() is non-empty.
  // Note that we should also propagate this up to not print @media rules
  // if all their rulesets are empty. Otherwise we'll fail the css_minify_test
  // which checks for idempotent minifications.
  switch (ruleset.type()) {
    case Css::Ruleset::RULESET:
      if (ruleset.selectors().is_dummy()) {
        Write(ruleset.selectors().bytes_in_original_buffer());
      } else {
        JoinMinify(ruleset.selectors(), ",");
      }
      Write("{");
      JoinMinify(ruleset.declarations(), ";");
      Write("}");
      break;
    case Css::Ruleset::UNPARSED_REGION:
      Minify(*ruleset.unparsed_region());
      break;
  }
}

void CssMinify::Minify(const Css::Selector& selector) {
  // Note Css::Selector == std::vector<Css::SimpleSelectors*>
  Css::Selector::const_iterator iter = selector.begin();
  if (iter != selector.end()) {
    bool isfirst = true;
    Minify(**iter, isfirst);
    ++iter;
    JoinMinifyIter(iter, selector.end(), "");
  }
}

void CssMinify::Minify(const Css::SimpleSelectors& sselectors, bool isfirst) {
  if (sselectors.combinator() == Css::SimpleSelectors::CHILD) {
    Write(">");
  } else if (sselectors.combinator() == Css::SimpleSelectors::SIBLING) {
    Write("+");
  } else if (!isfirst) {
    Write(" ");
  }
  // Note Css::SimpleSelectors == std::vector<Css::SimpleSelector*>
  JoinMinify(sselectors, "");
}

void CssMinify::Minify(const Css::SimpleSelector& sselector) {
  // SimpleSelector::ToString is already basically minified (and is escaped).
  Write(sselector.ToString());
}

namespace {

bool IsValueNormalIdentifier(const Css::Value& value) {
  return (value.GetLexicalUnitType() == Css::Value::IDENT &&
          value.GetIdentifier().ident() == Css::Identifier::NORMAL);
}

// See http://www.w3.org/TR/css3-values/#lengths : Lengths refer to
// distance measurements and are denoted by <length> in the property
// definitions. A length is a dimension. However, for zero lengths the
// unit identifier is optional (i.e. can be syntactically represented
// as the <number> 0).
//
// http://www.w3.org/TR/css3-values/#relative-lengths
// http://www.w3.org/TR/css3-values/#absolute-lengths
const char* kLengths[] = {
  "ch",
  "cm",
  "em",
  "ex",
  "in",
  "mm",
  "pc",
  "pt",
  "px",
  "q",
  "rem",
  "vh",
  "vmax",
  "vmin",
  "vw",
};

bool IsLength(const GoogleString& unit) {
  return std::binary_search(kLengths, kLengths + arraysize(kLengths),
                            unit);
}

}  // namespace

bool CssMinify::UnitsRequiredForValueZero(const GoogleString& unit) {
  // https://github.com/apache/incubator-pagespeed-mod/issues/1164 : Chrome does not
  // allow abbreviating 0s or 0% as 0.  It only allows that abbreviation for
  // lengths.
  //
  // https://github.com/apache/incubator-pagespeed-mod/issues/1261  See
  // https://www.w3.org/TR/CSS2/visudet.html#the-height-property
  //
  // https://github.com/apache/incubator-pagespeed-mod/issues/1538
  // retaining unit for zero value in calc function
  return (unit == "%") || !IsLength(unit) ||
          in_css_calc_function_;
}

void CssMinify::MinifyFont(const Css::Values& font_values) {
  CHECK_LE(5U, font_values.size());

  // font-style: defaults to normal
  if (!IsValueNormalIdentifier(*font_values.get(0))) {
    Minify(*font_values.get(0));
    Write(" ");
  }
  // font-variant: defaults to normal
  if (!IsValueNormalIdentifier(*font_values.get(1))) {
    Minify(*font_values.get(1));
    Write(" ");
  }
  // font-weight: defaults to normal
  if (!IsValueNormalIdentifier(*font_values.get(2))) {
    Minify(*font_values.get(2));
    Write(" ");
  }
  // font-size is required
  Minify(*font_values.get(3));
  // line-height: defaults to normal
  if (!IsValueNormalIdentifier(*font_values.get(4))) {
    Write("/");
    Minify(*font_values.get(4));
  }
  // font-family:
  for (int i = 5, n = font_values.size(); i < n; ++i) {
    Write(i == 5 ? " " : ",");
    Minify(*font_values.get(i));
  }
}

void CssMinify::Minify(const Css::Declaration& declaration) {
  if (declaration.prop() == Css::Property::UNPARSEABLE) {
    Write(declaration.bytes_in_original_buffer());
  } else {
    Write(Css::EscapeIdentifier(declaration.prop_text()));
    Write(":");
    switch (declaration.prop()) {
      case Css::Property::FONT_FAMILY:
        JoinMinify(*declaration.values(), ",");
        break;
      case Css::Property::FONT:
        // font: menu special case.
        if (declaration.values()->size() == 1) {
          JoinMinify(*declaration.values(), " ");
          // Normal font notation.
        } else if (declaration.values()->size() >= 5) {
          MinifyFont(*declaration.values());
        } else {
          handler_->Message(kError, "Unexpected number of values in "
                            "font declaration: %d",
                            static_cast<int>(declaration.values()->size()));
          ok_ = false;
        }
        break;
      default:
        // TODO(ashishk):unicode-range should get resolved to css property
        // enum.
        if (declaration.prop_text() == "unicode-range") {
          // https://github.com/apache/incubator-pagespeed-mod/issues/1572
          // space separator should not be there in unicode range value
          JoinMinify(*declaration.values(), "");
        } else {
          JoinMinify(*declaration.values(), " ");
        }
        break;
    }
    if (declaration.IsImportant()) {
      Write("!important");
    }
  }
}

void CssMinify::Minify(const Css::Value& value) {
  switch (value.GetLexicalUnitType()) {
    case Css::Value::NUMBER: {
      GoogleString buffer;
      StringPiece number_string;
      if (!value.bytes_in_original_buffer().empty()) {
        // All parsed values should have verbatim bytes set and we use them
        // to ensure we keep the original precision.
        number_string = value.bytes_in_original_buffer();
      } else {
        // Values added or modified outside of the parsing code need
        // to be converted to strings by us.
        buffer = StringPrintf("%.16g", value.GetFloatValue());
        number_string = buffer;
      }
      if (number_string.starts_with("0.")) {
        // Optimization: Strip "0.25" -> ".25".
        Write(number_string.substr(1));
      } else if (number_string.starts_with("-0.")) {
        // Optimization: Strip "-0.25" -> "-.25".
        Write("-");
        Write(number_string.substr(2));
      } else {
        // Otherwise just print the original string.
        Write(number_string);
      }

      // Optimization: Do not print units if value is 0.
      GoogleString unit = value.GetDimensionUnitText();
      if (!unit.empty() &&
          ((value.GetFloatValue() != 0) || UnitsRequiredForValueZero(unit))) {
        // Unit can be either "%" or an identifier.
        if (unit != "%") {
          unit = Css::EscapeIdentifier(unit);
        }
        Write(unit);
      }
      break;
    }
    case Css::Value::URI:
      Write("url(");
      WriteURL(value.GetStringValue());
      Write(")");
      break;
    case Css::Value::FUNCTION:
      if (Css::EscapeIdentifier(value.GetFunctionName()) == "calc") {
        in_css_calc_function_ = true;
      }
      Write(Css::EscapeIdentifier(value.GetFunctionName()));
      Write("(");
      Minify(*value.GetParametersWithSeparators());
      Write(")");
      in_css_calc_function_ = false;
      break;
    case Css::Value::RECT:
      Write("rect(");
      Minify(*value.GetParametersWithSeparators());
      Write(")");
      break;
    case Css::Value::COLOR:
      // TODO(sligocki): Can we assert, or might this happen in the wild?
      CHECK(value.GetColorValue().IsDefined());
      Write(HtmlColorUtils::MaybeConvertToCssShorthand(
          value.GetColorValue()));
      break;
    case Css::Value::STRING:
      if (!value.bytes_in_original_buffer().empty()) {
        // All parsed strings should have verbatim bytes set.
        // Note: bytes_in_original_buffer() contains quote chars.
        Write(value.bytes_in_original_buffer());
      } else {
        // Strings added or modified outside of the parsing code will need
        // to be serialized by us.
        Write("\"");
        Write(Css::EscapeString(value.GetStringValue()));
        Write("\"");
      }
      break;
    case Css::Value::IDENT:
      Write(Css::EscapeIdentifier(value.GetIdentifierText()));
      break;
    case Css::Value::COMMA:
      // TODO(sligocki): Do not add spaces around COMMA tokens.
      Write(",");
      break;
    case Css::Value::UNKNOWN:
      handler_->MessageS(kError, "Unknown attribute");
      ok_ = false;
      break;
    case Css::Value::DEFAULT:
      break;
  }
}

void CssMinify::Minify(const Css::FunctionParameters& parameters) {
  if (parameters.size() >= 1) {
    Minify(*parameters.value(0));
  }
  for (int i = 1, n = parameters.size(); i < n; ++i) {
    switch (parameters.separator(i)) {
      case Css::FunctionParameters::COMMA_SEPARATED:
        Write(",");
        break;
      case Css::FunctionParameters::SPACE_SEPARATED:
        Write(" ");
        break;
    }
    Minify(*parameters.value(i));
  }
}

void CssMinify::Minify(const Css::UnparsedRegion& unparsed_region) {
  Write(unparsed_region.bytes_in_original_buffer());
}


bool CssMinify::Equals(const Css::MediaQueries& a,
                       const Css::MediaQueries& b) const {
  if (a.size() != b.size()) {
    return false;
  }
  for (int i = 0, n = a.size(); i < n; ++i) {
    if (!Equals(*a.at(i), *b.at(i))) {
      return false;
    }
  }
  return true;
}

bool CssMinify::Equals(const Css::MediaQuery& a,
                       const Css::MediaQuery& b) const {
  if (a.qualifier() != b.qualifier() ||
      a.media_type() != b.media_type() ||
      a.expressions().size() != b.expressions().size()) {
    return false;
  }
  for (int i = 0, n = a.expressions().size(); i < n; ++i) {
    if (!Equals(a.expression(i), b.expression(i))) {
      return false;
    }
  }
  return true;
}

bool CssMinify::Equals(const Css::MediaExpression& a,
                       const Css::MediaExpression& b) const {
  if (a.name() != b.name() ||
      a.has_value() != b.has_value()) {
    return false;
  }
  if (a.has_value() && a.value() != b.value()) {
    return false;
  }
  return true;
}

}  // namespace net_instaweb
