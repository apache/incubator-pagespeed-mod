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



#include "util/gtl/stl_util.h"

#include "webutil/css/selector.h"

namespace Css {

//
// static
//

const HtmlTagIndex SimpleSelector::tagindex_;

//
// SimpleSelector factory methods
//

SimpleSelector* SimpleSelector::NewElementType(const UnicodeText& name) {
  HtmlTagEnum tag = static_cast<HtmlTagEnum>(
      tagindex_.FindHtmlTag(name.utf8_data(), name.utf8_length()));
  return new SimpleSelector(tag, name);
}

SimpleSelector* SimpleSelector::NewUniversal() {
    return new SimpleSelector(SimpleSelector::UNIVERSAL,
                              UnicodeText(), UnicodeText());
}

SimpleSelector* SimpleSelector::NewExistAttribute(
    const UnicodeText& attribute) {
  return new SimpleSelector(SimpleSelector::EXIST_ATTRIBUTE,
                            attribute, UnicodeText());
}

SimpleSelector* SimpleSelector::NewBinaryAttribute(
    Type type, const UnicodeText& attribute, const UnicodeText& value) {
  return new SimpleSelector(type, attribute, value);
}

static const char kClassText[] = "class";
SimpleSelector* SimpleSelector::NewClass(const UnicodeText& classname) {
  static const UnicodeText kClass =
    UTF8ToUnicodeText(kClassText, strlen(kClassText));
  return new SimpleSelector(SimpleSelector::CLASS,
                            kClass, classname);
}

static const char kIdText[] = "id";
SimpleSelector* SimpleSelector::NewId(const UnicodeText& id) {
  static const UnicodeText kId = UTF8ToUnicodeText(kIdText, strlen(kIdText));
  return new SimpleSelector(SimpleSelector::ID,
                            kId, id);
}

// sep is the separator. Either ":" or "::".
// See: http://www.w3.org/TR/CSS2/selector.html#pseudo-elements
//  and http://www.w3.org/TR/css3-selectors/#pseudo-elements
SimpleSelector* SimpleSelector::NewPseudoclass(
    const UnicodeText& pseudoclass, const UnicodeText& sep) {
  return new SimpleSelector(SimpleSelector::PSEUDOCLASS,
                            sep, pseudoclass);
}

SimpleSelector* SimpleSelector::NewLang(const UnicodeText& lang) {
  return new SimpleSelector(SimpleSelector::LANG,
                            UnicodeText(), lang);
}

//
// Some destructors that need STLDeleteElements() from stl_util.h
//

SimpleSelectors::~SimpleSelectors() { STLDeleteElements(this); }
Selector::~Selector() { STLDeleteElements(this); }
Selectors::~Selectors() { STLDeleteElements(this); }

}  // namespace
