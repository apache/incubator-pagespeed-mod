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

/* Computed positions: -k'1,3,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "htmlparse/html_name.gperf"

// html_name.cc is automatically generated from html_name.gperf.

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
#line 22 "htmlparse/html_name.gperf"
struct KeywordMap {const char* name; net_instaweb::HtmlName::Keyword keyword;};
#include <string.h>

#define TOTAL_KEYWORDS 137
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 22
#define MIN_HASH_VALUE 7
#define MAX_HASH_VALUE 209
/* maximum key range = 203, duplicates = 0 */

#ifndef GPERF_DOWNCASE
#define GPERF_DOWNCASE 1
static unsigned char gperf_downcase[256] =
  {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
     30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
     45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255
  };
#endif

#ifndef GPERF_CASE_STRNCMP
#define GPERF_CASE_STRNCMP 1
static int
gperf_case_strncmp (register const char *s1, register const char *s2, register unsigned int n)
{
  for (; n > 0;)
    {
      unsigned char c1 = gperf_downcase[(unsigned char)*s1++];
      unsigned char c2 = gperf_downcase[(unsigned char)*s2++];
      if (c1 != 0 && c1 == c2)
        {
          n--;
          continue;
        }
      return (int)c1 - (int)c2;
    }
  return 0;
}
#endif

class KeywordMapper
{
private:
  static inline unsigned int hash (const char *str, unsigned int len);
public:
  static const struct KeywordMap *Lookup (const char *str, unsigned int len);
};

inline unsigned int
KeywordMapper::hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210,  50,
       40,  31,  25,  11,  10, 210, 210, 210, 210, 210,
      210, 210, 210, 210,   2,  81,   9,  98,   3,   4,
       38,  63,  17,  60,  85,  32, 109,  27,  18,  53,
       23,  29,  26,   5,   1,  21,  69,  79, 133,  99,
        5, 210, 210, 210, 210, 210, 210,  81,   9,  98,
        3,   4,  38,  63,  17,  60,  85,  32, 109,  27,
       18,  53,  23,  29,  26,   5,   1,  21,  69,  79,
      133,  99,   5, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
      210, 210, 210, 210, 210, 210, 210
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[2]+1];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]+1];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static const struct KeywordMap kHtmlNameTable[] =
  {
    {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 64 "htmlparse/html_name.gperf"
    {"dt",                     HtmlName::kDt},
#line 133 "htmlparse/html_name.gperf"
    {"rt",                     HtmlName::kRt},
#line 53 "htmlparse/html_name.gperf"
    {"dd",                     HtmlName::kDd},
    {""}, {""}, {""},
#line 135 "htmlparse/html_name.gperf"
    {"script",                 HtmlName::kScript},
    {""},
#line 145 "htmlparse/html_name.gperf"
    {"style",                  HtmlName::kStyle},
#line 60 "htmlparse/html_name.gperf"
    {"disabled",               HtmlName::kDisabled},
    {""},
#line 54 "htmlparse/html_name.gperf"
    {"declare",                HtmlName::kDeclare},
#line 141 "htmlparse/html_name.gperf"
    {"shape",                  HtmlName::kShape},
#line 44 "htmlparse/html_name.gperf"
    {"charset",                HtmlName::kCharset},
    {""},
#line 46 "htmlparse/html_name.gperf"
    {"class",                  HtmlName::kClass},
#line 137 "htmlparse/html_name.gperf"
    {"seamless",               HtmlName::kSeamless},
    {""},
#line 29 "htmlparse/html_name.gperf"
    {"address",                HtmlName::kAddress,},
#line 149 "htmlparse/html_name.gperf"
    {"td",                     HtmlName::kTd},
#line 150 "htmlparse/html_name.gperf"
    {"test",                   HtmlName::kTest},
    {""},
#line 138 "htmlparse/html_name.gperf"
    {"section",                HtmlName::kSection},
#line 132 "htmlparse/html_name.gperf"
    {"rp",                     HtmlName::kRp},
#line 50 "htmlparse/html_name.gperf"
    {"command",                HtmlName::kCommand},
#line 143 "htmlparse/html_name.gperf"
    {"span",                   HtmlName::kSpan},
#line 134 "htmlparse/html_name.gperf"
    {"scoped",                 HtmlName::kScoped},
#line 30 "htmlparse/html_name.gperf"
    {"alt",                    HtmlName::kAlt},
#line 139 "htmlparse/html_name.gperf"
    {"select",                 HtmlName::kSelect},
    {""},
#line 58 "htmlparse/html_name.gperf"
    {"details",                HtmlName::kDetails},
#line 59 "htmlparse/html_name.gperf"
    {"dir",                    HtmlName::kDir},
#line 140 "htmlparse/html_name.gperf"
    {"selected",               HtmlName::kSelected},
#line 153 "htmlparse/html_name.gperf"
    {"th",                     HtmlName::kTh},
#line 32 "htmlparse/html_name.gperf"
    {"article",                HtmlName::kArticle},
#line 129 "htmlparse/html_name.gperf"
    {"required",               HtmlName::kRequired},
    {""},
#line 37 "htmlparse/html_name.gperf"
    {"autofocus",              HtmlName::kAutofocus},
    {""},
#line 36 "htmlparse/html_name.gperf"
    {"autocomplete",           HtmlName::kAutocomplete},
#line 106 "htmlparse/html_name.gperf"
    {"muted",                  HtmlName::kMuted},
#line 104 "htmlparse/html_name.gperf"
    {"method",                 HtmlName::kMethod},
#line 155 "htmlparse/html_name.gperf"
    {"tr",                     HtmlName::kTr},
#line 152 "htmlparse/html_name.gperf"
    {"tfoot",                  HtmlName::kTfoot},
#line 45 "htmlparse/html_name.gperf"
    {"checked",                HtmlName::kChecked},
#line 65 "htmlparse/html_name.gperf"
    {"enctype",                HtmlName::kEnctype},
#line 121 "htmlparse/html_name.gperf"
    {"p",                      HtmlName::kP},
    {""},
#line 49 "htmlparse/html_name.gperf"
    {"colspan",                HtmlName::kColspan},
    {""},
#line 105 "htmlparse/html_name.gperf"
    {"multiple",               HtmlName::kMultiple},
#line 156 "htmlparse/html_name.gperf"
    {"type",                   HtmlName::kType},
#line 116 "htmlparse/html_name.gperf"
    {"onload",                 HtmlName::kOnload},
    {""},
#line 48 "htmlparse/html_name.gperf"
    {"colgroup",               HtmlName::kColgroup},
#line 113 "htmlparse/html_name.gperf"
    {"object",                 HtmlName::kObject},
#line 111 "htmlparse/html_name.gperf"
    {"noscript",               HtmlName::kNoscript},
#line 51 "htmlparse/html_name.gperf"
    {"content",                HtmlName::kContent},
#line 115 "htmlparse/html_name.gperf"
    {"onclick",                HtmlName::kOnclick},
#line 125 "htmlparse/html_name.gperf"
    {"param",                  HtmlName::kParam},
#line 154 "htmlparse/html_name.gperf"
    {"thead",                  HtmlName::kThead},
#line 119 "htmlparse/html_name.gperf"
    {"option",                 HtmlName::kOption},
#line 52 "htmlparse/html_name.gperf"
    {"controls",               HtmlName::kControls},
#line 110 "htmlparse/html_name.gperf"
    {"noresize",               HtmlName::kNoresize},
#line 35 "htmlparse/html_name.gperf"
    {"audio",                  HtmlName::kAudio},
#line 79 "htmlparse/html_name.gperf"
    {"h6",                     HtmlName::kH6},
#line 78 "htmlparse/html_name.gperf"
    {"h5",                     HtmlName::kH5},
#line 126 "htmlparse/html_name.gperf"
    {"pre",                    HtmlName::kPre},
#line 118 "htmlparse/html_name.gperf"
    {"optgroup",               HtmlName::kOptgroup},
#line 80 "htmlparse/html_name.gperf"
    {"head",                   HtmlName::kHead},
#line 100 "htmlparse/html_name.gperf"
    {"loop",                   HtmlName::kLoop},
#line 136 "htmlparse/html_name.gperf"
    {"scrolling",              HtmlName::kScrolling},
#line 107 "htmlparse/html_name.gperf"
    {"name",                   HtmlName::kName},
#line 142 "htmlparse/html_name.gperf"
    {"source",                 HtmlName::kSource},
#line 72 "htmlparse/html_name.gperf"
    {"frame",                  HtmlName::kFrame},
#line 66 "htmlparse/html_name.gperf"
    {"event",                  HtmlName::kEvent},
#line 117 "htmlparse/html_name.gperf"
    {"open",                   HtmlName::kOpen},
#line 55 "htmlparse/html_name.gperf"
    {"defaultchecked",         HtmlName::kDefaultchecked},
#line 56 "htmlparse/html_name.gperf"
    {"defaultselected",        HtmlName::kDefaultselected},
#line 71 "htmlparse/html_name.gperf"
    {"formnovalidate",         HtmlName::kFormnovalidate},
#line 77 "htmlparse/html_name.gperf"
    {"h4",                     HtmlName::kH4},
#line 84 "htmlparse/html_name.gperf"
    {"hr",                     HtmlName::kHr},
#line 98 "htmlparse/html_name.gperf"
    {"li",                     HtmlName::kLi},
#line 88 "htmlparse/html_name.gperf"
    {"id",                     HtmlName::kId},
#line 28 "htmlparse/html_name.gperf"
    {"a",                      HtmlName::kA},
#line 97 "htmlparse/html_name.gperf"
    {"language",               HtmlName::kLanguage},
#line 76 "htmlparse/html_name.gperf"
    {"h3",                     HtmlName::kH3},
#line 83 "htmlparse/html_name.gperf"
    {"hgroup",                 HtmlName::kHgroup},
#line 130 "htmlparse/html_name.gperf"
    {"reversed",               HtmlName::kReversed},
#line 102 "htmlparse/html_name.gperf"
    {"menu",                   HtmlName::kMenu},
#line 69 "htmlparse/html_name.gperf"
    {"for",                    HtmlName::kFor},
#line 57 "htmlparse/html_name.gperf"
    {"defer",                  HtmlName::kDefer},
#line 70 "htmlparse/html_name.gperf"
    {"form",                   HtmlName::kForm},
#line 89 "htmlparse/html_name.gperf"
    {"iframe",                 HtmlName::kIframe},
#line 81 "htmlparse/html_name.gperf"
    {"header",                 HtmlName::kHeader},
#line 75 "htmlparse/html_name.gperf"
    {"h2",                     HtmlName::kH2},
#line 33 "htmlparse/html_name.gperf"
    {"aside",                  HtmlName::kAside,},
#line 147 "htmlparse/html_name.gperf"
    {"tag",                    HtmlName::kTag},
#line 144 "htmlparse/html_name.gperf"
    {"src",                    HtmlName::kSrc},
#line 91 "htmlparse/html_name.gperf"
    {"indeterminate",          HtmlName::kIndeterminate},
#line 39 "htmlparse/html_name.gperf"
    {"base",                   HtmlName::kBase},
#line 101 "htmlparse/html_name.gperf"
    {"media",                  HtmlName::kMedia},
#line 73 "htmlparse/html_name.gperf"
    {"frameborder",            HtmlName::kFrameborder},
#line 67 "htmlparse/html_name.gperf"
    {"fieldset",               HtmlName::kFieldset},
#line 61 "htmlparse/html_name.gperf"
    {"display",                HtmlName::kDisplay},
#line 74 "htmlparse/html_name.gperf"
    {"h1",                     HtmlName::kH1},
    {""},
#line 120 "htmlparse/html_name.gperf"
    {"other",                  HtmlName::kOther},
#line 63 "htmlparse/html_name.gperf"
    {"dl",                     HtmlName::kDl},
#line 99 "htmlparse/html_name.gperf"
    {"link",                   HtmlName::kLink},
#line 34 "htmlparse/html_name.gperf"
    {"async",                  HtmlName::kAsync},
#line 68 "htmlparse/html_name.gperf"
    {"footer",                 HtmlName::kFooter},
#line 158 "htmlparse/html_name.gperf"
    {"valuetype",              HtmlName::kValuetype},
#line 92 "htmlparse/html_name.gperf"
    {"input",                  HtmlName::kInput},
#line 127 "htmlparse/html_name.gperf"
    {"readonly",               HtmlName::kReadonly},
    {""}, {""},
#line 103 "htmlparse/html_name.gperf"
    {"meta",                   HtmlName::kMeta},
#line 95 "htmlparse/html_name.gperf"
    {"keytype",                HtmlName::kKeytype},
#line 42 "htmlparse/html_name.gperf"
    {"br",                     HtmlName::kBr},
    {""},
#line 146 "htmlparse/html_name.gperf"
    {"table",                  HtmlName::kTable},
    {""}, {""},
#line 93 "htmlparse/html_name.gperf"
    {"ismap",                  HtmlName::kIsmap},
#line 31 "htmlparse/html_name.gperf"
    {"area",                   HtmlName::kArea},
#line 27 "htmlparse/html_name.gperf"
    {"?xml",                   HtmlName::kXml},
#line 114 "htmlparse/html_name.gperf"
    {"ol",                     HtmlName::kOl},
#line 40 "htmlparse/html_name.gperf"
    {"blockquote",             HtmlName::kBlockquote},
    {""},
#line 38 "htmlparse/html_name.gperf"
    {"autoplay",               HtmlName::kAutoplay},
#line 94 "htmlparse/html_name.gperf"
    {"keygen",                 HtmlName::kKeygen},
    {""},
#line 85 "htmlparse/html_name.gperf"
    {"href",                   HtmlName::kHref},
#line 159 "htmlparse/html_name.gperf"
    {"video",                  HtmlName::kVideo},
#line 47 "htmlparse/html_name.gperf"
    {"col",                    HtmlName::kCol},
#line 43 "htmlparse/html_name.gperf"
    {"button",                 HtmlName::kButton},
#line 128 "htmlparse/html_name.gperf"
    {"rel",                    HtmlName::kRel},
    {""},
#line 112 "htmlparse/html_name.gperf"
    {"novalidate",             HtmlName::kNovalidate},
#line 96 "htmlparse/html_name.gperf"
    {"lang",                   HtmlName::kLang},
#line 148 "htmlparse/html_name.gperf"
    {"tbody",                  HtmlName::kTbody},
    {""}, {""}, {""},
#line 82 "htmlparse/html_name.gperf"
    {"height",                 HtmlName::kHeight},
    {""},
#line 163 "htmlparse/html_name.gperf"
    {"xmp",                    HtmlName::kXmp},
#line 62 "htmlparse/html_name.gperf"
    {"div",                    HtmlName::kDiv},
    {""},
#line 109 "htmlparse/html_name.gperf"
    {"nohref",                 HtmlName::kNohref},
    {""},
#line 161 "htmlparse/html_name.gperf"
    {"width",                  HtmlName::kWidth},
#line 87 "htmlparse/html_name.gperf"
    {"http-equiv",             HtmlName::kHttpEquiv},
    {""},
#line 123 "htmlparse/html_name.gperf"
    {"pagespeed_lazy_src",     HtmlName::kPagespeedLazySrc},
#line 131 "htmlparse/html_name.gperf"
    {"rowspan",                HtmlName::kRowspan},
    {""},
#line 124 "htmlparse/html_name.gperf"
    {"pagespeed_low_res_src",  HtmlName::kPagespeedLowResSrc},
#line 122 "htmlparse/html_name.gperf"
    {"pagespeed_high_res_src", HtmlName::kPagespeedHighResSrc},
#line 160 "htmlparse/html_name.gperf"
    {"wbr",                    HtmlName::kWbr},
#line 90 "htmlparse/html_name.gperf"
    {"img",                    HtmlName::kImg},
#line 162 "htmlparse/html_name.gperf"
    {"wrap",                   HtmlName::kWrap},
    {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {""},
#line 157 "htmlparse/html_name.gperf"
    {"ul",                     HtmlName::kUl},
    {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {""},
#line 86 "htmlparse/html_name.gperf"
    {"html",                   HtmlName::kHtml},
    {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {""}, {""}, {""},
#line 108 "htmlparse/html_name.gperf"
    {"nav",                    HtmlName::kNav},
#line 41 "htmlparse/html_name.gperf"
    {"body",                   HtmlName::kBody},
    {""}, {""}, {""},
#line 151 "htmlparse/html_name.gperf"
    {"textarea",               HtmlName::kTextarea}
  };

const struct KeywordMap *
KeywordMapper::Lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = kHtmlNameTable[key].name;

          if ((((unsigned char)*str ^ (unsigned char)*s) & ~32) == 0 && !gperf_case_strncmp (str, s, len) && s[len] == '\0')
            return &kHtmlNameTable[key];
        }
    }
  return 0;
}
#line 164 "htmlparse/html_name.gperf"


HtmlName::Keyword HtmlName::Lookup(const StringPiece& keyword) {
  const KeywordMap* keyword_map = KeywordMapper::Lookup(keyword.data(),
                                                        keyword.size());
  if (keyword_map != NULL) {
    return keyword_map->keyword;
  }
  return HtmlName::kNotAKeyword;
}

bool HtmlName::Iterator::AtEnd() const {
  return index_ > MAX_HASH_VALUE;
}

void HtmlName::Iterator::Next() {
  DCHECK(!AtEnd());
  ++index_;
  while (!AtEnd() && (*(kHtmlNameTable[index_].name) == '\0')) {
    ++index_;
  }
}

const char* HtmlName::Iterator::name() const {
  DCHECK(!AtEnd());
  return kHtmlNameTable[index_].name;
}

HtmlName::Keyword HtmlName::Iterator::keyword() const {
  DCHECK(!AtEnd());
  return kHtmlNameTable[index_].keyword;
}

int HtmlName::num_keywords() {
  return TOTAL_KEYWORDS;
}

}  // namespace net_instaweb
