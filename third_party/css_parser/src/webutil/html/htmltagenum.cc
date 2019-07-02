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



#include "webutil/html/htmltagenum.h"

#include "base/macros.h"
#include "base/stringprintf.h"

const char* HtmlTagEnumNames[] =
{    "Unknown",
     "A", "Abbr", "Acronym", "Address", "Applet",
     "Area", "B", "Base", "Basefont", "Bdo",
     "Big", "Blockquote", "Body", "Br", "Button",
     "Caption", "Center", "Cite", "Code", "Col",
     "Colgroup", "Dd", "Del", "Dfn", "Dir",
     "Div", "Dl", "Dt", "Em", "Fieldset",
     "Font", "Form", "Frame", "Frameset", "H1",
     "H2", "H3", "H4", "H5", "H6", "Head",
     "Hr", "Html", "I", "Iframe", "Img",
     "Input", "Ins", "Isindex", "Kbd", "Label",
     "Legend", "Li", "Link", "Map", "Menu",
     "Meta", "Noframes", "Noscript", "Object",
     "Ol", "Optgroup", "Option", "P", "Param",
     "Pre", "Q", "S", "Samp", "Script",
     "Select", "Small", "Span", "Strike",
     "Strong", "Style", "Sub", "Sup", "Table",
     "Tbody", "Td", "Textarea", "Tfoot", "Th",
     "Thead", "Title", "Tr", "Tt",
     "U", "Ul", "Var",
  // Empty tag
     "ZeroLength",
  // Used in repository/lexer/html_lexer.cc
     "!--", "Blink",
  // Used in repository/parsers/base/handler-parser.cc
     "Embed", "Marquee",
  // Legacy backwards-compatible tags mentioned in HTML5.
     "Nobr", "Wbr", "Bgsound", "Image",
     "Listing", "Noembed", "Plaintext", "Spacer",
     "Xmp",
  // From Netscape Navigator 4.0
     "Ilayer", "Keygen", "Layer", "Multicol", "Nolayer", "Server",
  // !doctype
     "!Doctype",
  // Legacy tag used mostly by Russian sites.
     "Noindex",
  // Old style comments,
     "!Comment",
  // New tags in HTML5.
     "Article", "Aside", "Audio", "Bdi", "Canvas", "Command", "Datalist",
     "Details", "Figcaption", "Figure", "Footer", "Header", "Hgroup", "Mark",
     "Meter", "Nav", "Output", "Progress", "Rp", "Rt", "Ruby", "Section",
     "Source", "Summary", "Time", "Track", "Video",
  // Other HTML5 tags.
     "Data", "Main", "Rb", "Rtc", "Template",
  // The HTML5 picture tag.
     "Picture",
};

COMPILE_ASSERT(arraysize(HtmlTagEnumNames) == kHtmlTagBuiltinMax,
               ForgotToAddTagToHtmlTagEnumNames);

const char* HtmlTagName(HtmlTagEnum tag) {
  if (tag < kHtmlTagBuiltinMax) {
    return HtmlTagEnumNames[tag];
  } else {
    return NULL;
  }
}

string HtmlTagNameOrUnknown(int i) {
  if (i >= 0 && i < kHtmlTagBuiltinMax) {
    return HtmlTagEnumNames[i];
  } else {
    return StringPrintf("UNKNOWN%d", i);
  }
}
