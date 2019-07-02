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


#ifndef PAGESPEED_KERNEL_HTML_DOCTYPE_H_
#define PAGESPEED_KERNEL_HTML_DOCTYPE_H_

#include "pagespeed/kernel/base/string_util.h"  // for StringPiece

namespace net_instaweb {
struct ContentType;

// Holds an HTML Doctype declaration, providing a parsing mechanism and queries
// for properties.
class DocType {
 public:
  DocType() : doctype_(UNKNOWN) {}
  DocType(const DocType& src) : doctype_(src.doctype_) {}
  ~DocType() {}

  DocType& operator=(const DocType& src) {
    if (&src != this) {
      doctype_ = src.doctype_;
    }
    return *this;
  }

  bool operator==(const DocType& other) const {
    return doctype_ == other.doctype_;
  }

  bool operator!=(const DocType& other) const {
    return doctype_ != other.doctype_;
  }

  // Return true iff this is a known XHTML doctype (of some version).
  bool IsXhtml() const;
  // Return true iff this is an HTML 5 or XHTML 5 doctype.
  bool IsVersion5() const;
  // TODO(mdsteele): Add more such methods as necessary.

  static const DocType kUnknown;
  static const DocType kHTML5;
  static const DocType kHTML4Strict;
  static const DocType kHTML4Transitional;
  static const DocType kXHTML5;
  static const DocType kXHTML11;
  static const DocType kXHTML10Strict;
  static const DocType kXHTML10Transitional;

  // Given the contents of an HTML directive and the content type of the file
  // it appears in, update this DocType to match that specified by the
  // directive and return true.  If the directive is not a doctype directive,
  // return false and don't alter the DocType.
  bool Parse(const StringPiece& directive,
             const ContentType& content_type);

 private:
  enum DocTypeEnum {
    UNKNOWN = 0,
    HTML_5,
    HTML_4_STRICT,
    HTML_4_TRANSITIONAL,
    XHTML_5,
    XHTML_1_1,
    XHTML_1_0_STRICT,
    XHTML_1_0_TRANSITIONAL,
    OTHER_XHTML,
  };

  explicit DocType(DocTypeEnum doctype) : doctype_(doctype) {}

  DocTypeEnum doctype_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_DOCTYPE_H_
