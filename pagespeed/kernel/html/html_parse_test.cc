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


// Unit-test the html reader/writer to ensure that a few tricky
// constructs come through without corruption.

#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/html/disable_test_filter.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/explicit_close_tag.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_event.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/html/html_testing_peer.h"
#include "pagespeed/kernel/html/html_writer_filter.h"

using testing::UnorderedElementsAre;

namespace net_instaweb {

class HtmlParseTest : public HtmlParseTestBase {
 protected:
  // Returns the contents wrapped in a Div.
  GoogleString Div(const StringPiece& text) {
    return StrCat("<div>", text, "</div>");
  }

  // For tag-pairs that auto-close, we expect the appearance
  // of tag2 to automatically close tag1.
  void ExpectAutoClose(const char* tag1, const char* tag2) {
    GoogleString test_case = StrCat("auto_close_", tag1, "_", tag2);
    ValidateExpected(
        test_case,
        Div(StrCat("<", tag1, ">x<", tag2, ">y")),
        Div(StrCat("<", tag1, ">x</", tag1, "><",
                   StrCat(tag2, ">y</", tag2, ">"))));
  }

  // For 2 tags that do not have a specified auto-close relationship,
  // we expect the appearance of tag2 to nest inside tag1.
  void ExpectNoAutoClose(const char* tag1, const char* tag2) {
    GoogleString test_case = StrCat("no_auto_close_", tag1, "_", tag2);
    ValidateExpected(
        test_case,
        Div(StrCat("<", tag1, ">x<", tag2, ">y")),
        Div(StrCat("<", tag1, ">x<", tag2, ">y</",
                   StrCat(tag2, "></", tag1, ">"))));
  }

  virtual bool AddBody() const { return true; }

  // Sends the input through the HtmlParse filter chain, flushing
  // at flush_index.  Leaves resulting output in output_buffer_.
  void ParseWithFlush(StringPiece input, int flush_index) {
    GoogleString this_id = StringPrintf("http://test.com/%d", flush_index);
    output_buffer_.clear();
    html_parse_.StartParse(this_id);
    html_parse_.ParseText(input.substr(0, flush_index));
    html_parse_.Flush();
    html_parse_.ParseText(input.substr(flush_index));
    html_parse_.FinishParse();
  }
};

class HtmlParseTestNoBody : public HtmlParseTestBase {
  virtual bool AddBody() const { return false; }
};

TEST_F(HtmlParseTest, AvoidFalseXmlComment) {
  ValidateNoChanges("avoid_false_xml_comment",
     "<script type=\"text/javascript\">\n"
     "// <!-- this looks like a comment but is not\n"
     "</script>");
}

TEST_F(HtmlParseTest, RetainBogusEndTag) {
  ValidateNoChanges("bogus_end_tag",
     "<script language=\"JavaScript\" type=\"text/javascript\">\n"
     "<!--\n"
     "var s = \"</retain_bogus_end_tag>\";\n"
     "// -->\n"
     "</script>");
}

TEST_F(HtmlParseTest, AmpersandInHref) {
  // Note that we will escape the "&" in the href.
  ValidateNoChanges("ampersand_in_href",
      "<a href=\"http://myhost.com/path?arg1=val1&arg2=val2\">Hello</a>");
}

TEST_F(HtmlParseTest, BooleanSpaceCloseInTag) {
  ValidateExpected("bool_space_close", "<a b >foo</a>", "<a b>foo</a>");
  ValidateNoChanges("bool_close", "<a b>foo</a>");
  ValidateExpected("space_close_sq", "<a b='c' >foo</a>", "<a b='c'>foo</a>");
  ValidateExpected("space_close_dq",
                   "<a b=\"c\" >foo</a>", "<a b=\"c\">foo</a>");
  ValidateExpected("space_close_nq", "<a b=c >foo</a>", "<a b=c>foo</a>");
  // Distilled from http://www.gougou.com/
  // Unclear exactly what we should do here, maybe leave it as it was without
  // the space?
  ValidateExpected("allow_semicolon",
                   "<a onclick='return m(this)'; >foo</a>",
                   "<a onclick='return m(this)' ;>foo</a>");
}

TEST_F(HtmlParseTest, EmbeddedNuls) {
  const char kHtml[] = "<script att\0r></script>";
  // Note: STATIC_STRLEN won't stop at embedded null.
  ValidateNoChanges("inner_mess", GoogleString(kHtml, STATIC_STRLEN(kHtml)));

  const char kHtml2[] = "<script\0y></script>";
  // Note: STATIC_STRLEN won't stop at embedded null.
  ValidateNoChanges("inner_mess2",
                    GoogleString(kHtml2, STATIC_STRLEN(kHtml2)));
}

class AttrValuesSaverFilter : public EmptyHtmlFilter {
 public:
  AttrValuesSaverFilter() { }

  virtual void StartElement(HtmlElement* element) {
    const HtmlElement::AttributeList& attrs = element->attributes();
    for (HtmlElement::AttributeConstIterator i(attrs.begin());
         i != attrs.end(); ++i) {
      const char* value = i->DecodedValueOrNull();
      if (i->decoding_error()) {
        value_ += "<ERROR>";
      } else if (value == NULL) {
        value_ += "(null)";
      } else {
        value_ += value;
      }
    }
  }

  const GoogleString& value() { return value_; }
  virtual const char* Name() const { return "attr_saver"; }

 private:
  GoogleString value_;

  DISALLOW_COPY_AND_ASSIGN(AttrValuesSaverFilter);
};

TEST_F(HtmlParseTest, EscapedSingleQuote) {
  AttrValuesSaverFilter attr_saver;
  html_parse_.AddFilter(&attr_saver);
  Parse("escaped_single_quote",
        "<img src='my&#39;single_quoted_image.jpg'/>");
  EXPECT_EQ("my'single_quoted_image.jpg", attr_saver.value());
}

TEST_F(HtmlParseTest, AttrDecodeError) {
  AttrValuesSaverFilter attr_saver;
  html_parse_.AddFilter(&attr_saver);
  Parse("attr_not_decodable", "<img src='muñecos'/>");
  EXPECT_EQ("<ERROR>", attr_saver.value());
}

TEST_F(HtmlParseTest, UnclosedQuote) {
  // In this test, the system automatically closes the 'a' tag, which
  // didn't really get closed in the input text.  The exact syntax
  // of the expected results not critical, as long as the parser recovers
  // and does not crash.
  //
  // TODO(jmarantz): test error reporting.
  ValidateNoChanges("unclosed_quote",
     "<div>\n"
     "  <a href=\"http://myhost.com/path?arg1=val1&arg2=val2>Hello</a>\n"
     "</div>\n"
     "<p>next token</p>"
     "</body></html>\n"
     "\"></a></div>");
}

TEST_F(HtmlParseTest, NestedDivInBr) {
  ValidateNoChanges("nested_div_in_br",
     "<br><div>hello</div></br>");
}

// bug 2465145 - Sequential defaulted attribute tags lost
TEST_F(HtmlParseTest, SequentialDefaultedTagsLost) {
  // This test cannot work with libxml, but since we use our own
  // parser we can make it work.  See
  // https://bugzilla.gnome.org/show_bug.cgi?id=611655
  ValidateNoChanges("sequential_defaulted_attribute_tags_lost",
      "<select>\n"
      "  <option value=\"&amp;cat=244\">Other option</option>\n"
      "  <option value selected style=\"color: #ccc;\">Default option"
      "</option>\n"
      "</select>");

  // Illegal attribute "http://www.yahoo.com", per HTML5, is two attributes:
  // http: and "yahoo.com", with the slashes going into the ether.
  // (This is also how Chrome and Firefox parse it.)
  ValidateExpected(
      "yahoo",
      "<a href=\"#\" http://www.yahoo.com class=\"a b\">yahoo</a>",
      "<a href=\"#\" http: www.yahoo.com class=\"a b\">yahoo</a>");

  // Here's another interesting thing from the bug testcase.
  // Specifying a literal "&" without a recognized sequence
  // following it gets parsed correctly by libxml2, and then
  // re-encoded by our writer as &amp;.  That's fine; let's
  // make sure that doesn't change.
  ValidateNoChanges("amp_cat",
      "<option value=\"&cat=244\">other</option>");
}

// bug 2465201 : some html constructs do not need ';' termination.
// Fixed by providing own lexer.
TEST_F(HtmlParseTest, UnterminatedTokens) {
  // the termination semicolons should be added in the output.
  ValidateNoChanges("unterminated_tokens",
      "<p>Look at the non breaking space: \"&nbsp\"</p>");
}

// bug 2467040 : keep ampersands and quotes encoded
TEST_F(HtmlParseTest, EncodeAmpersandsAndQuotes) {
  ValidateNoChanges("ampersands_in_text",
      "<p>This should be a string '&amp;amp;' not a single ampersand.</p>");
  ValidateNoChanges("ampersands_in_values",
      "<img alt=\"This should be a string '&amp;amp;' "
      "not a single ampersand.\"/>");
  ValidateNoChanges("quotes",
      "<p>Clicking <a href=\"javascript: alert(&quot;Alert works!&quot;);\">"
      "here</a> should pop up an alert box.</p>");
}

// bug 2508334 : encoding unicode in general
TEST_F(HtmlParseTest, EncodeUnicode) {
  ValidateNoChanges("unicode_in_text",
      "<p>Non-breaking space: '&nbsp;'</p>\n"
      "<p>Alpha: '&alpha;'</p>\n"
      "<p>Unicode #54321: '&#54321;'</p>\n");
}

TEST_F(HtmlParseTest, ImplicitExplicitClose) {
  // The lexer/printer preserves the input syntax, making it easier
  // to diff inputs & outputs.
  //
  // TODO(jmarantz): But we can have a rewrite pass that eliminates
  // the superfluous "/>".
  ValidateNoChanges("one_brief_one_implicit_input",
      "<input type=\"text\" name=\"username\">"
      "<input type=\"password\" name=\"password\"/>");
}

TEST_F(HtmlParseTest, OpenBracketAfterQuote) {
  // Note: even though it looks like two input elements, in practice
  // it's parsed as one.
  const char input[] =
      "<input type=\"text\" name=\"username\""
      "<input type=\"password\" name=\"password\"/>";
  const char expected[] =
      "<input type=\"text\" name=\"username\""
      " <input type=\"password\" name=\"password\"/>";
      // Extra space 'between' attributes'
  ValidateExpected("open_bracket_after_quote", input, expected);
}

TEST_F(HtmlParseTest, OpenBracketUnquoted) {
  // '<' after unquoted attr value.
  // This is just a malformed attribute name, not a start of a new tag.
  const char input[] =
      "<input type=\"text\" name=username"
      "<input type=\"password\" name=\"password\"/>";
  ValidateNoChanges("open_bracket_unquoted", input);
}

TEST_F(HtmlParseTest, OpenBracketAfterEquals) {
  // '<' after equals sign. This is actually an attribute value,
  // not a start of a new tag.
  const char input[] =
      "<input type=\"text\" name="
      "<input type=\"password\" name=\"password\"/>";
  ValidateNoChanges("open_brack_after_equals", input);
}

TEST_F(HtmlParseTest, OpenBracketAfterName) {
  // '<' after after attr name.
  const char input[] =
      "<input type=\"text\" name"
      "<input type=\"password\" name=\"password\"/>";
  ValidateNoChanges("open_brack_after_name", input);
}

class HtmlParseTestNoBodyNoHtml : public HtmlParseTestNoBody {
 protected:
  virtual bool AddHtmlTags() const { return false; }

  void CheckOutput(int start_index, int end_index,
                   const GoogleString& input,
                   const GoogleString& expected_output) {
    for (int i = start_index; i < end_index; ++i) {
      SetupWriter();
      html_parse()->set_size_limit(i);
      html_parse()->StartParse("http://test.com/in.html");
      // Flush after every character.
      for (int j = 0; j < input.size(); ++j) {
        GoogleString x;
        x.push_back(input[j]);
        html_parse()->ParseText(StringPiece(x));
        html_parse()->Flush();
      }
      html_parse()->FinishParse();
      EXPECT_EQ(expected_output, output_buffer_);
    }
  }
};

TEST_F(HtmlParseTestNoBodyNoHtml, SizeLimit) {
  static const char input[] =
      "<html>"  // 6 chars
      "<input type=\"text\"/>"  // 20 chars
      "<script type=\"text/javascript\">alert('123');</script>"  // 53 chars
      "<!--[if IE]>...<![endif]-->"  // 27 chars
      "<table><tr><td>blah</td></tr></table>"  // 37 chars
      "</html>";  // 7 chars
  ValidateNoChanges("no_limit", input);

  static const char output_when_break_in_html[] =
      "<html></html>";

  for (int i = 1; i < 150; ++i) {
    // With no flushes, the output is just <html></html>
    html_parse_.set_size_limit(i);
    ValidateExpected("break_in_input", input,
                     output_when_break_in_html);
  }

  // Now test with flushes injected.

  CheckOutput(1, 6, input, output_when_break_in_html);

  static const char output_when_break_in_input[] =
      "<html><input type=\"text\"/></html>";
  CheckOutput(6, 26, input, output_when_break_in_input);

  static const char output_with_break_in_script_tag[] =
      "<html><input type=\"text\"/>"
      "<script type=\"text/javascript\"></script>"
      "</html>";
  CheckOutput(26, 57, input, output_with_break_in_script_tag);

  static const char output_with_break_in_script_text_or_later[] =
      "<html><input type=\"text\"/>"
      "<script type=\"text/javascript\">alert('123');</script>"
      "</html>";
  CheckOutput(57, 79, input, output_with_break_in_script_text_or_later);

  static const char output_with_break_in_comment[] =
      "<html><input type=\"text\"/>"
      "<script type=\"text/javascript\">alert('123');</script>"
      "<!--[if IE]>...<![endif]-->"
      "<table></table>"
      "</html>";
  CheckOutput(79, 113, input, output_with_break_in_comment);

  static const char output_with_break_in_tr[] =
      "<html><input type=\"text\"/>"
      "<script type=\"text/javascript\">alert('123');</script>"
      "<!--[if IE]>...<![endif]-->"
      "<table><tr></tr></table>"
      "</html>";
  CheckOutput(113, 117, input, output_with_break_in_tr);

  static const char output_with_break_in_td[] =
      "<html><input type=\"text\"/>"
      "<script type=\"text/javascript\">alert('123');</script>"
      "<!--[if IE]>...<![endif]-->"
      "<table><tr><td></td></tr></table>"
      "</html>";
  CheckOutput(117, 121, input, output_with_break_in_td);

  static const char output_with_break_in_td_text[] =
      "<html><input type=\"text\"/>"
      "<script type=\"text/javascript\">alert('123');</script>"
      "<!--[if IE]>...<![endif]-->"
      "<table><tr><td>blah</td></tr></table>"
      "</html>";
  CheckOutput(121, 160, input, output_with_break_in_td_text);
}

TEST_F(HtmlParseTest, OpenBracketAfterSpace) {
  // '<' after after unquoted attr value. Here name<input is an attribute
  // name.
  const char input[] =
      "<input type=\"text\" "
      "<input type=\"password\" name=\"password\"/>";
  ValidateNoChanges("open_brack_after_name", input);
}

TEST_F(HtmlParseTest, AutoClose) {
  ExplicitCloseTag close_tags;
  html_parse_.AddFilter(&close_tags);

  // Cover the simple cases.  E.g. dd is closed by tr, but not dd.
  ExpectNoAutoClose("dd", "tr");
  ExpectAutoClose("dd", "dd");

  ExpectAutoClose("dt", "dd");
  ExpectAutoClose("dt", "dt");
  ExpectNoAutoClose("dt", "rp");

  ExpectAutoClose("li", "li");
  ExpectNoAutoClose("li", "dt");

  ExpectAutoClose("optgroup", "optgroup");
  ExpectNoAutoClose("optgroup", "rp");

  ExpectAutoClose("option", "optgroup");
  ExpectAutoClose("option", "option");
  ExpectNoAutoClose("option", "rp");

  // <p> has an outrageous number of tags that auto-close it.
  ExpectNoAutoClose("p", "tr");  // tr is not listed in the auto-closers for p.
  ExpectAutoClose("p", "address");  // first closer of 28.
  ExpectAutoClose("p", "h2");       // middle closer of 28.
  ExpectAutoClose("p", "ul");       // last closer of 28.

  // Cover the remainder of the cases.
  ExpectAutoClose("rp", "rt");
  ExpectAutoClose("rp", "rp");
  ExpectNoAutoClose("rp", "dd");

  ExpectAutoClose("rt", "rt");
  ExpectAutoClose("rt", "rp");
  ExpectNoAutoClose("rt", "dd");

  ExpectAutoClose("tbody", "tbody");
  ExpectAutoClose("tbody", "tfoot");
  ExpectNoAutoClose("tbody", "dd");

  ExpectAutoClose("td", "td");
  ExpectAutoClose("td", "th");
  ExpectNoAutoClose("td", "rt");

  ExpectAutoClose("tfoot", "tbody");
  ExpectNoAutoClose("tfoot", "dd");

  ExpectAutoClose("th", "td");
  ExpectAutoClose("th", "th");
  ExpectNoAutoClose("th", "rt");

  ExpectAutoClose("thead", "tbody");
  ExpectAutoClose("thead", "tfoot");
  ExpectNoAutoClose("thead", "dd");

  ExpectAutoClose("tr", "tr");
  ExpectNoAutoClose("tr", "td");

  // http://www.w3.org/TR/html5/the-end.html#misnested-tags:-b-i-b-i


  // TODO(jmarantz): add more tests related to formatting keywords.
}

TEST_F(HtmlParseTest, BogusComment) {
  ValidateNoChanges("what_php",
                    "<?php include('includes/_pagebottom.tpl.php'); ?>");

  ValidateNoChanges("bad break", "</\na>");
}

namespace {

class AnnotatingHtmlFilter : public EmptyHtmlFilter {
 public:
  AnnotatingHtmlFilter() : annotate_flush_(false) {}
  virtual ~AnnotatingHtmlFilter() {}

  virtual void StartElement(HtmlElement* element) {
    StrAppend(&buffer_, (buffer_.empty() ? "+" : " +"), element->name_str());

    bool first = true;
    const HtmlElement::AttributeList& attrs = element->attributes();
    for (HtmlElement::AttributeConstIterator i(attrs.begin());
         i != attrs.end(); ++i) {
      const HtmlElement::Attribute& attr = *i;
      StrAppend(&buffer_, (first ? ":" : ","), attr.name_str());
      const char* value = attr.DecodedValueOrNull();
      if (attr.decoding_error()) {
        StrAppend(&buffer_, "=<ERROR>");
      } else if (value != NULL) {
        StrAppend(&buffer_, "=", attr.quote_str(), value, attr.quote_str());
      }
      first = false;
    }
  }
  virtual void EndElement(HtmlElement* element) {
    StrAppend(&buffer_, " -", element->name_str());
    switch (element->style()) {
      case HtmlElement::AUTO_CLOSE:      buffer_ += "(a)"; break;
      case HtmlElement::IMPLICIT_CLOSE:  buffer_ += "(i)"; break;
      case HtmlElement::EXPLICIT_CLOSE:  buffer_ += "(e)"; break;
      case HtmlElement::BRIEF_CLOSE:     buffer_ += "(b)"; break;
      case HtmlElement::UNCLOSED:        buffer_ += "(u)"; break;
      case HtmlElement::INVISIBLE:       buffer_ += "(I)"; break;
    }
  }
  virtual void Characters(HtmlCharactersNode* characters) {
    StrAppend(&buffer_, (buffer_.empty() ? "'" : " '"), characters->contents(),
              "'");
  }

  virtual const char* Name() const { return "AnnotatingHtmlFilter"; }

  const GoogleString& buffer() const { return buffer_; }
  void Clear() { buffer_.clear(); }

  virtual void Flush() {
    if (annotate_flush_) {
      buffer_ += "[F]";
    }
  }

  void set_annotate_flush(bool x) { annotate_flush_ = x; }

 private:
  bool annotate_flush_;
  GoogleString buffer_;
};

}  // namespace

class HtmlAnnotationTest : public HtmlParseTestNoBody {
 protected:
  virtual void SetUp() {
    HtmlParseTestNoBody::SetUp();
    html_parse_.AddFilter(&annotation_);
  }

  const GoogleString& annotation() { return annotation_.buffer(); }
  void ResetAnnotation() { annotation_.Clear(); }
  virtual bool AddHtmlTags() const { return false; }

 protected:
  AnnotatingHtmlFilter annotation_;
};

TEST_F(HtmlAnnotationTest, CorrectTaggify) {
  // Under HTML5 rules (and recent Chrome and FF practice), something like
  // <foo</bar> makes an element named foo<, with attribute named bar.
  // (See 12.2.4.10 Tag name state).
  //
  // However, we have to be careful not to turn just anything following <
  // into an element name, since sometimes there are <'s which are
  // meant to just be less than signs.
  //
  ValidateNoChanges("no_taggify_digit", "<p>1<2</p>");
  EXPECT_EQ("+p '1<2' -p(e)", annotation());
  ResetAnnotation();

  ValidateNoChanges("no_taggify_unicode", "<p>☃<☕</p>");
  EXPECT_EQ("+p '☃<☕' -p(e)", annotation());
  ResetAnnotation();

  ValidateExpected("letter",
                   "<p>x<y</p>", "<p>x<y< p>");  // lost the / since 'p' is attr.
  EXPECT_EQ("+p 'x' +y<:p -y<(u) -p(u)", annotation());
  ResetAnnotation();

  ValidateExpected("taggify_letter+digit",
                   "<p>x1<y2</p>", "<p>x1<y2< p>");
  EXPECT_EQ("+p 'x1' +y2<:p -y2<(u) -p(u)", annotation());
  ResetAnnotation();

  ValidateExpected("taggify_letter+unicode", "<p>x☃<y☕</p>",
                   "<p>x☃<y☕< p>");  // no / since p is attr on a y☕< element.
  EXPECT_EQ("+p 'x☃' +y☕<:p -y☕<(u) -p(u)", annotation());
  ResetAnnotation();

  ValidateNoChanges("no_taggify_digit+letter", "<p>1x<2y</p>");
  EXPECT_EQ("+p '1x<2y' -p(e)", annotation());
  ResetAnnotation();

  ValidateNoChanges("no_taggify_unicode+letter", "<p>☃x<☕y</p>");
  EXPECT_EQ("+p '☃x<☕y' -p(e)", annotation());
  ResetAnnotation();

  // Found on http://www.taobao.com/
  // Don't turn <1... -> <1...>
  ValidateNoChanges("taobao", "<a>1+1<1母婴全场加1元超值购</a>");
  EXPECT_EQ("+a '1+1<1母婴全场加1元超值购' -a(e)", annotation());
  ResetAnnotation();
}

TEST_F(HtmlAnnotationTest, WeirdAttributes) {
  // Just about everything can be an attribute
  ValidateNoChanges("weird_attr", "<a ,=\"foo\">");
  EXPECT_EQ("+a:,=\"foo\" -a(u)", annotation());
  ResetAnnotation();

  // ... even an equal sign
  ValidateNoChanges("weird_attr_equal", "<a ==\"foo\">");
  EXPECT_EQ("+a:==\"foo\" -a(u)", annotation());
  ResetAnnotation();
}

TEST_F(HtmlAnnotationTest, WeirdCloseCase) {
  // </> is nothing useful, but we preserve it as a literal.
  ValidateNoChanges("close_nothing", "</><foo>");
  EXPECT_EQ("'</>' +foo -foo(u)", annotation());
  ResetAnnotation();

  // <foo / > isn't an attempt at self-close, it just has a stray /
  // we can't represent.
  ValidateExpected("not_self_close", "<foo / >", "<foo>");
  EXPECT_EQ("+foo -foo(u)", annotation());
  ResetAnnotation();

  // <foo /> is a self-close.
  ValidateExpected("self_close", "<foo />", "<foo/>");
  EXPECT_EQ("+foo -foo(b)", annotation());
  ResetAnnotation();
}

TEST_F(HtmlAnnotationTest, UnbalancedMarkup) {
  // The second 'tr' closes the first one, and our HtmlWriter will not
  // implicitly close 'tr' because IsImplicitlyClosedTag is false, so
  // the markup is changed to add the missing tr.
  ValidateNoChanges("unbalanced_markup",
                    "<font><tr><i><font></i></font><tr></font>");

  // We use this (hopefully) self-explanatory annotation format to indicate
  // what's going on in the parse.
  EXPECT_EQ("+font -font(a) +tr +i +font -font(u) -i(e) '</font>' -tr(a) +tr "
            "'</font>' -tr(u)",
            annotation());
}

TEST_F(HtmlAnnotationTest, StrayCloseTr) {
  ValidateNoChanges("stray_tr",
                    "<table><tr><table></tr></table></tr></table>");

  // We use this (hopefully) self-explanatory annotation format to indicate
  // what's going on in the parse.
  EXPECT_EQ("+table +tr +table '</tr>' -table(e) -tr(e) -table(e)",
            annotation());
}

TEST_F(HtmlAnnotationTest, IClosedByOpenTr) {
  ValidateNoChanges("unclosed_i_tag", "<tr><i>a<tr>b");
  EXPECT_EQ("+tr +i 'a' -i(a) -tr(a) +tr 'b' -tr(u)", annotation());

  // TODO(jmarantz): morlovich points out that this is nowhere near
  // how a browser will handle this stuff... For a nighmarish testcase, try:
  //     data:text/html,<table><tr><td><i>a<tr>b
  //
  // The 'a' gets rendered in italics *after* the b.
  //
  // See also:
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/
  // the-end.html#unexpected-markup-in-tables
  //
  // But note that these 2 are the same and do what I expect:
  //
  // data:text/html,<table><tr><td><i>a</td></tr></table>b
  // data:text/html,<table><tr><td><i>a</table>b
  //
  // the 'a' is italicized but the 'b' is not.  If I omit the 'td'
  // then the 'b' gets italicized.  This implies I suppose that 'i' is
  // closed by td but is not closed by tr or table.  And it is indeed
  // closed by the *implicit* closing of td.

  // http://www.w3.org/TR/html5/the-end.html#misnested-tags:-b-i-b-i
}

TEST_F(HtmlAnnotationTest, INotClosedByOpenTableExplicit) {
  ValidateNoChanges("explicit_close_tr", "<i>a<table><tr></tr></table>b");
  EXPECT_EQ("+i 'a' +table +tr -tr(e) -table(e) 'b' -i(u)", annotation());
}

TEST_F(HtmlAnnotationTest, INotClosedByOpenTableImplicit) {
  ValidateNoChanges("implicit_close_tr", "<i>a<table><tr></table>b");
  EXPECT_EQ("+i 'a' +table +tr -tr(u) -table(e) 'b' -i(u)", annotation());
}

TEST_F(HtmlAnnotationTest, AClosedByBInLi) {
  ValidateNoChanges("a_closed_by_b", "<li><a href='x'></b>");
  EXPECT_EQ("+li +a:href='x' '</b>' -a(u) -li(u)", annotation());
}

TEST_F(HtmlAnnotationTest, BClosedByTd) {
  ValidateNoChanges("b_closed_by_td", "<table><tr><td><b>1</table></b>");

  // The <b> gets closed by the </td>, which is automatically closed by
  // the td, which is automatically closed by the tr, which is automatically
  // closed by the tbody, which is automatically closed by the "</table>".
  // The actual "</b>" that appears here doesn't close any open tags, so
  // its rendered as literal characters.
  //
  // TODO(jmarantz): consider adding a new event-type to represent bogus
  // tags rather than using Characters.
  EXPECT_EQ("+table +tr +td +b '1' -b(u) -td(u) -tr(u) -table(e) '</b>'",
            annotation());
}

TEST_F(HtmlAnnotationTest, BNotClosedByTable) {
  ValidateNoChanges(
      "a_closed_by_b",
      "<table><tbody><tr><b><td>hello</tr></tbody></table>World</b>");

  // We do not create the same annotation Chrome does in this case.  Opening up
  // the inspector on
  // data:text/html,<table><tbody><tr><b><td>hello</tr></tbody></table>World</b>
  // shows us (ignoring html, head, and body tags for brevity):
  //      <b></b>
  //      <table>
  //        <tbody>
  //          <tr>
  //            <td>hello</td>
  //          </td>
  //        </tbody>
  //      </table>
  //      <b>World</b>
  // For us to replicate this structure, we'd have to move the 'b' tag ahead of
  // the <table> opening tag.  To do this we would need to buffer tables until
  // they reached the end-table tag.  This does not appear to be a good
  // tradeoff as tables might be large and buffering them would impact
  // the UX for all sites, as a defense against bad markup and filters that
  // care deeply about the structure of formatting elements in illegal DOM
  // positions.
  //
  // But note that this malformed markup will in fact pass through
  // parsing & serialization with byte accuracy.
}

TEST_F(HtmlAnnotationTest, StrayCloseTrInTable) {
  ValidateNoChanges("stray_close_tr",
                    "<div><table><tbody><td>1</td></tr></tbody></table></div>");
  EXPECT_EQ("+div +table +tbody +td '1' -td(e) '</tr>' -tbody(e) -table(e) "
            "-div(e)", annotation());
}

TEST_F(HtmlAnnotationTest, StrayCloseTrInTableWithUnclosedTd) {
  ValidateNoChanges("stray_close_tr_unclosed_td",
                    "<tr><table><td>1</tr></table>");
  EXPECT_EQ("+tr +table +td '1</tr>' -td(u) -table(e) -tr(u)", annotation());
  // TODO(jmarantz): the above is not quite DOM-accurate.  A 'tr' will
  // actually be synthesized around the <td>.  To solve this and
  // maintain byte accuracy we must synthesize an HtmlElement whose
  // opening-tag is invisible, and create a map that requires <td>
  // elements to be enclosed in <tr> etc.  See, in Chrome,
  // data:text/html,<tr><table><td>1</tr></table>
}

TEST_F(HtmlAnnotationTest, OverlappingStyleTags) {
  ValidateNoChanges("overlapping_style_tags", "n<b>b<i>bi</b>i</i>n");

  // TODO(jmarantz): The behavior of this sequence is well-specified, but
  // is not currently implemented by PSA.  We should have
  // EXPECT_EQ("'n' +b 'b' +i 'bi' -i(u) -b(e) +i* 'i' -i(e) 'n'",
  //           annotation());
  // Note that we will need to render a synthetic <i> that shows up in our
  // DOM tree but does not get serialized.  We have no current representation
  // for that, but we could easily add a bool to HtmlElement to suppress the
  // serialization of the open tag.  Above that's represented by "+i*".
  //
  // But we actually get this, which does not have the 'i' in italics.
  EXPECT_EQ("'n' +b 'b' +i 'bi' -i(u) -b(e) 'i</i>n'", annotation());

  // There is no real drawback to implementing this; but at the moment
  // no filters are likely to care.
}

TEST_F(HtmlAnnotationTest, AClosedByP) {
  ValidateNoChanges("a_closed_by_p", "<P>This is a <A>link<P>More");

  // According to Chrome("data:text/html,<P>This is a <A>link<P>More") the
  // structure should be something like this:
  //     "+p 'This is a' +a link -a -p +p +a more -a -p"
  // In this fashion a&p overlap together in a fashion similar to bold and
  // italic.
  //
  // But we actually product this markup:
  EXPECT_EQ("+P 'This is a ' +A 'link' +P 'More' -P(u) -A(u) -P(u)",
            annotation());
}

TEST_F(HtmlAnnotationTest, PFont) {
  ValidateNoChanges("p_font", "<P><FONT>a<P>b</FONT>");

  // TODO(jmarantz): The second <P> should force the close of
  // the first one, despite the intervening <font>.  In other words
  // we need to keep track of which formatting elements are active:
  // <p> does not nest but I suppose <font> likely does.
  //
  // Chrome("data:text/html,<P><FONT>a<P>b</FONT>") yields
  // "<p><font>a</font</p><p><font><b></font></p>"
  EXPECT_EQ("+P +FONT 'a' +P 'b' -P(u) -FONT(e) -P(u)", annotation());
}

TEST_F(HtmlAnnotationTest, HtmlTbodyCol) {
  // The spaces before the tag names are invalid.  Chrome parses these as
  // literals; our behavior is consistent.
  ValidateNoChanges("html_tbody_col", "< HTML> < TBODY> < COL SPAN=999999999>");
  EXPECT_EQ("'< HTML> < TBODY> < COL SPAN=999999999>'", annotation());
}

TEST_F(HtmlAnnotationTest, WeirdAttrQuotes) {
  // Note that in the expected results, a space was inserted before
  // 'position:absolute' and before 'Windings'.  I think this is correct.
  //
  // TODO(jmarantz): check in Chrome.
  ValidateExpected("weird_attr_quotes",
                    "<DIV STYLE=\"top:214px; left:139px;\""
                    "position:absolute; font-size:26px;\">"
                    "<NOBR><SPAN STYLE=\"font-family:\"Wingdings 2\";\">"
                   "</SPAN></NOBR></DIV>",
                   "<DIV STYLE=\"top:214px; left:139px;\" "
                   "position:absolute; font-size:26px;\">"
                   "<NOBR><SPAN STYLE=\"font-family:\" Wingdings 2\";\">"
                   "</SPAN></NOBR></DIV>");
  EXPECT_EQ("+DIV:STYLE=\"top:214px; left:139px;\",position:absolute;,"
            "font-size:26px;\" +NOBR "
            "+SPAN:STYLE=\"font-family:\",Wingdings,2\";\" "
            "-SPAN(e) -NOBR(e) -DIV(e)", annotation());
}

TEST_F(HtmlAnnotationTest, Misc) {
  //
  // 1. This is <B>bold, <I>bold italic, </b>italic, </i>normal text
  // 2. <P>This is a <A>link<P>More
  // 3. <P><FONT>a<P>b</FONT>
  // 7. <img title=="><script>alert('foo')</script>">
  // 8. < HTML> < TBODY> < COL SPAN=999999999>
  // 9. <DIV STYLE="top:214px; left:139px; position:absolute; font-size:26px;">
  //    <NOBR><SPAN STYLE="font-family:"Wingdings 2";"></SPAN></NOBR></DIV>
  // 10. <a href="http://www.cnn.com/"' title="cnn.com">cnn</a>
  // 11. do <![if !supportLists]>not<![endif]> lose this text
  // 12. <table><tr><td>row1<tr><td>row2</td>
  // 13. <table><tr><td>foo<td>bar<tr><td>baz<td>boo</table>
  // 14. <p>The quick <strong>brown fox</strong></p>\njumped over the\n
  //     <p>lazy</strong> dog.</p>
  // 15. <p> paragraph <h1> heading </h1>
  // 16. <a href="h">1<a>2</a></a>
  ValidateNoChanges("quote_balance", "<img title=\"><script>alert('foo')"
                    "</script>\">");
  EXPECT_EQ("+img:title=\"><script>alert('foo')</script>\" -img(i)",
            annotation());
}

TEST_F(HtmlAnnotationTest, DoubleEquals) {
  // Note that the attr-value is not in fact a quoted string.  The second
  // "=" begins the attr-value and its terminated by the ">".  The script
  // is not in the quote.  The closing quote and > are stray and rendered
  // as characters in our DOM.  We are byte accurate.  This behavior
  // was hand-confirmed as consistent with Chrome by typing
  //      data:text/html,<img title=="><script>alert('foo')</script>">
  // into the URL bar on 12/13/2011.  The "alert" popped up which is
  // consistent with the dom annotation below.
  ValidateNoChanges("double_equals",
                    "<img title==\"><script>alert('foo')</script>\">");
  EXPECT_EQ("+img:title==\" -img(i) +script 'alert('foo')' -script(e) '\">'",
            annotation());
}

TEST_F(HtmlAnnotationTest, AttrEqStartWithSlash) {
  // Note the "/>" here does *not* briefly end the 'body'; it's part of the
  // attribute.  Verified with chrome using
  // data:text/html,<body title=/>hello</body>
  ValidateNoChanges("attr_eq_starts_with_slash", "<body title=/>1</body>");
  EXPECT_EQ("+body:title=/ '1' -body(e)", annotation());
}

TEST_F(HtmlAnnotationTest, AttrEqEndsWithSlash) {
  // Note again the "/>" here does *not* briefly end the 'body'; it's part of
  // the attribute.  Verified with chrome using
  // data:text/html,<body title=x/>hello</body>
  ValidateNoChanges("attr_eq_ends_with_slash", "<body title=x/></body>");
  EXPECT_EQ("+body:title=x/ -body(e)", annotation());
}

TEST_F(HtmlAnnotationTest, TableForm) {
  ValidateNoChanges("table_form", "<table><form><input></table><input></form>");
  EXPECT_EQ("+table +form +input -input(i) -form(u) -table(e)"
            " +input -input(i) '</form>'",
            annotation());
}

TEST_F(HtmlAnnotationTest, ComplexQuotedAttribute) {
  ValidateNoChanges("complex_quoted_attr",
                    "<div x='\\'><img onload=alert(42)"
                    "src=http://json.org/img/json160.gif>'></div>");
  EXPECT_EQ("+div:x='\\' "
            "+img:onload=alert(42)src=http://json.org/img/json160.gif "
            "-img(i) ''>' -div(e)", annotation());
}

TEST_F(HtmlAnnotationTest, DivNbsp) {
  ValidateNoChanges("div_nbsp",
                    "<div&nbsp &nbsp style=\\-\\mo\\z\\-b\\i\\nd\\in\\g:\\url("
                    "//business\\i\\nfo.co.uk\\/labs\\/xbl\\/xbl\\.xml\\#xss)"
                    ">");
  EXPECT_EQ("'<div&nbsp &nbsp style=\\-\\mo\\z\\-b\\i\\nd\\in\\g:\\"
            "url(//business\\i\\nfo.co.uk\\/labs\\/xbl\\/xbl\\.xml\\#xss)>'",
            annotation());
}

TEST_F(HtmlAnnotationTest, ExtraQuote) {
  ValidateExpected(
      "extra_quote",
      "<a href=\"http://www.cnn.com/\"' title=\"cnn.com\">cnn</a>",
      "<a href=\"http://www.cnn.com/\" ' title=\"cnn.com\">cnn</a>");
}

TEST_F(HtmlAnnotationTest, TrNesting) {
  ValidateNoChanges("nesting", "<tr><td><tr a=b><td c=d></td></tr>");
  EXPECT_EQ("+tr +td -td(a) -tr(a) +tr:a=b +td:c=d -td(e) -tr(e)",
            annotation());
}

TEST_F(HtmlAnnotationTest, AttrEndingWithOpenAngle) {
  ValidateNoChanges("weird_attr", "<script src=foo<bar>Content");
  EXPECT_EQ("+script:src=foo<bar 'Content' -script(u)", annotation());
}

TEST_F(HtmlAnnotationTest, ScriptQuirkBasic) {
  ValidateNoChanges("script_quirk_1",
                    "<script><!--<script></script>a</script>b");
  EXPECT_EQ("+script '<!--<script></script>a' -script(e) 'b'", annotation());

  ResetAnnotation();
  ValidateNoChanges("script_quirk_2",
                    "<script><!--</script>a</script>b");
  EXPECT_EQ("+script '<!--' -script(e) 'a</script>b'", annotation());

  ResetAnnotation();
  ValidateNoChanges("script_quirk_3",
                    "<script><script></script>a</script>b");
  EXPECT_EQ("+script '<script>' -script(e) 'a</script>b'", annotation());

  ResetAnnotation();
  ValidateNoChanges("script_quirk_4",
                    "<script><!--<script>--></script>a</script>b");
  EXPECT_EQ("+script '<!--<script>-->' -script(e) 'a</script>b'", annotation());
}

TEST_F(HtmlAnnotationTest, ScriptQuirkCloseAttr) {
  // HTML5 script parsing is weird in that </script> actually gets attribute
  // parsing.
  ValidateExpected("script_quirk_close",
                   "<script></script a=\"foo>\">Bar",
                   "<script></script>Bar");
  EXPECT_EQ("+script -script(e) 'Bar'", annotation());

  ResetAnnotation();
  ValidateExpected("script_quirk_close2",
                   "<script></script a=\"foo>\" bar=\'>' bax>Bar",
                   "<script></script>Bar");
  EXPECT_EQ("+script -script(e) 'Bar'", annotation());


  ResetAnnotation();
  ValidateExpected("script_quirk_close_slash",
                   "<script></script a=\"foo>\"/>Bar",
                   "<script></script>Bar");
  EXPECT_EQ("+script -script(e) 'Bar'", annotation());
}

TEST_F(HtmlAnnotationTest, ScriptQuirkBriefClose) {
  // HTML5 script parsing --- closing </style />
  ValidateExpected("script_quirk_close_brief",
                   "<script></script/>Bar",
                   "<script></script>Bar");
  EXPECT_EQ("+script -script(e) 'Bar'", annotation());

  ResetAnnotation();
  ValidateExpected("script_quirk_close_brief",
                   "<script></script /foo>Bar",
                   "<script></script>Bar");
  EXPECT_EQ("+script -script(e) 'Bar'", annotation());
}

// TODO(jmarantz): fix this case; we lose the stray "=".
// TEST_F(HtmlAnnotationTest, StrayEq) {
//   ValidateNoChanges("stray_eq", "<a href='foo.html'=>b</a>");
//   EXPECT_EQ("+a:href=foo.html -a(e)", annotation());
// }

TEST_F(HtmlAnnotationTest, FlushDoesNotBreakCharacterBlock) {
  annotation_.set_annotate_flush(true);
  html_parse_.StartParse("http://test.com/blank_flush.html");
  html_parse_.ParseText("<div></div>");  // will get flushed.
  html_parse_.ParseText("bytes:");       // will not get flushed till the end.
  html_parse_.Flush();
  html_parse_.ParseText(":more:");
  html_parse_.Flush();
  html_parse_.ParseText(":still more:");
  html_parse_.Flush();
  html_parse_.ParseText(":final bytes:");
  html_parse_.FinishParse();
  EXPECT_STREQ(
      "+div -div(e)[F][F][F] 'bytes::more::still more::final bytes:'[F]",
      annotation());
}

TEST_F(HtmlAnnotationTest, FlushDoesNotBreakScriptTag) {
  annotation_.set_annotate_flush(true);
  html_parse_.StartParse("http://test.com/blank_flush.html");
  html_parse_.ParseText("<script>");
  html_parse_.Flush();
  html_parse_.ParseText("a=b;");
  html_parse_.Flush();
  html_parse_.ParseText("c=d;");
  html_parse_.Flush();
  html_parse_.ParseText("</scr");
  html_parse_.Flush();
  html_parse_.ParseText("ipt><script>");
  html_parse_.Flush();
  html_parse_.ParseText("e=f;");
  html_parse_.Flush();
  html_parse_.ParseText("g=h;");
  // No explicit </script> but the lexer will help us close it.
  html_parse_.FinishParse();
  EXPECT_STREQ("[F][F][F][F] +script 'a=b;c=d;' -script(e)[F][F]"
               " +script 'e=f;g=h;' -script(u)[F]",  // "(u)" for unclosed.
               annotation());
}

TEST_F(HtmlAnnotationTest, FlushDoesNotBreakScriptTagWithComment) {
  SetupWriter();
  annotation_.set_annotate_flush(true);
  html_parse_.StartParse("http://test.com/blank_flush.html");
  html_parse_.ParseText("<script>");
  html_parse_.InsertComment("c1");
  html_parse_.Flush();
  html_parse_.ParseText("a=b;");
  html_parse_.Flush();
  html_parse_.ParseText("</script><script>");
  html_parse_.InsertComment("c2");
  html_parse_.Flush();
  html_parse_.ParseText("</script>");
  html_parse_.FinishParse();
  EXPECT_STREQ("[F][F] +script 'a=b;' -script(e)[F] +script -script(e)[F]",
               annotation());
  EXPECT_STREQ("<!--c1--><script>a=b;</script><!--c2--><script></script>",
               output_buffer_);
}

TEST_F(HtmlAnnotationTest, FlushDoesNotBreakStyleTag) {
  annotation_.set_annotate_flush(true);
  html_parse_.StartParse("http://test.com/blank_flush.html");
  html_parse_.ParseText("<style>");
  html_parse_.Flush();
  html_parse_.ParseText(".blue {color: ");
  html_parse_.Flush();
  html_parse_.ParseText("blue;}");
  html_parse_.Flush();
  html_parse_.ParseText("</style>");
  html_parse_.FinishParse();
  EXPECT_STREQ("[F][F][F] +style '.blue {color: blue;}' -style(e)[F]",
               annotation());
}

TEST_F(HtmlAnnotationTest, UnclosedScriptOnly) {
  SetupWriter();
  annotation_.set_annotate_flush(true);
  html_parse_.StartParse("http://test.com/blank_flush.html");
  html_parse_.ParseText("<script>");
  html_parse_.FinishParse();

  // Note that we will get an EndElement callback.  See -script(u) in annotation.
  // However we will not insert a </script> in the output, since there was none
  // in the input.
  EXPECT_STREQ("+script -script(u)[F]", annotation());
  EXPECT_STREQ("<script>", output_buffer_);
}

TEST_F(HtmlAnnotationTest, UnclosedScriptOnlyWithFlush) {
  SetupWriter();
  annotation_.set_annotate_flush(true);
  html_parse_.StartParse("http://test.com/blank_flush.html");
  html_parse_.ParseText("<script>");
  html_parse_.Flush();
  html_parse_.FinishParse();

  // Note that we will get an EndElement callback.  See -script(u) in annotation.
  // However we will not insert a </script> in the output, since there was none
  // in the input.
  EXPECT_STREQ("[F] +script -script(u)[F]", annotation());
  EXPECT_STREQ("<script>", output_buffer_);
}

TEST_F(HtmlAnnotationTest, NulInAttrName) {
  // Tests that we don't crash with an embedded NUL in an attribute name.
  // Without '%compare-lengths' in html_name.gperf, this would fail when built
  // with asan.
  SetupWriter();
  html_parse_.StartParse("http://test.com/nul_in_attr.html");
  html_parse_.ParseText("<img src");
  html_parse_.ParseText(StringPiece("\0", 1));
  html_parse_.ParseText("file:-1675375991 />");
  html_parse_.FinishParse();
}

TEST_F(HtmlParseTest, MakeName) {
  EXPECT_EQ(0, HtmlTestingPeer::symbol_table_size(&html_parse_));

  // Empty names are a corner case that we hope does not crash.  Note
  // that empty-string atoms are special-cased in the symbol table
  // and require no new allocated bytes.
  {
    HtmlName empty = html_parse_.MakeName("");
    EXPECT_EQ(0, HtmlTestingPeer::symbol_table_size(&html_parse_));
    EXPECT_EQ(HtmlName::kNotAKeyword, empty.keyword());
    EXPECT_EQ("", empty.value());
  }

  // When we make a name using its enum, there should be no symbol table growth.
  HtmlName body_symbol = html_parse_.MakeName(HtmlName::kBody);
  EXPECT_EQ(0, HtmlTestingPeer::symbol_table_size(&html_parse_));
  EXPECT_EQ(HtmlName::kBody, body_symbol.keyword());

  // When we make a name using the canonical form (all-lower-case) there
  // should still be no symbol table growth.
  HtmlName body_canonical = html_parse_.MakeName("body");
  EXPECT_EQ(0, HtmlTestingPeer::symbol_table_size(&html_parse_));
  EXPECT_EQ(HtmlName::kBody, body_canonical.keyword());

  // But when we introduce a new capitalization, we want to retain the
  // case, even though we do html keyword matching.  We will have to
  // store the new form in the symbol table so we'll be allocating
  // some bytes, including the nul terminator.
  HtmlName body_new_capitalization = html_parse_.MakeName("Body");
  EXPECT_EQ(4, HtmlTestingPeer::symbol_table_size(&html_parse_));
  EXPECT_EQ(HtmlName::kBody, body_new_capitalization.keyword());

  // Make a name out of something that is not a keyword.
  // This should also increase the symbol-table size.
  HtmlName non_keyword = html_parse_.MakeName("hiybbprqag");
  EXPECT_EQ(14, HtmlTestingPeer::symbol_table_size(&html_parse_));
  EXPECT_EQ(HtmlName::kNotAKeyword, non_keyword.keyword());

  // Empty names are a corner case that we hope does not crash.  Note
  // that empty-string atoms are special-cased in the symbol table
  // and require no new allocated bytes.
  {
    HtmlName empty = html_parse_.MakeName("");
    EXPECT_EQ(14, HtmlTestingPeer::symbol_table_size(&html_parse_));
    EXPECT_EQ(HtmlName::kNotAKeyword, empty.keyword());
    EXPECT_EQ("", empty.value());
  }
}

// bug 2508140 : <noscript> in <head>
TEST_F(HtmlParseTestNoBody, NoscriptInHead) {
  // Some real websites (ex: google.com) have <noscript> in the <head> even
  // though this is technically illegal according to the HTML4 spec.
  // We should support the case in use.
  ValidateNoChanges("noscript_in_head",
      "<head><noscript><title>You don't have JS enabled :(</title>"
      "</noscript></head>");
}

TEST_F(HtmlParseTestNoBody, NoCaseFold) {
  // Case folding is off by default.  However, we don't keep the
  // closing-tag separate in the IR so we will always make that
  // match.
  ValidateExpected("no_case_fold",
                   "<DiV><Other xY='AbC' Href='dEf'>Hello</OTHER></diV>",
                   "<DiV><Other xY='AbC' Href='dEf'>Hello</Other></DiV>");
  // Despite the fact that we retain case, in our IR, and the cases did not
  // match between opening and closing tags, there should be no messages
  // warning about unmatched tags.
  EXPECT_EQ(0, message_handler_.TotalMessages());
}

TEST_F(HtmlParseTestNoBody, CaseFold) {
  SetupWriter();
  html_writer_filter_->set_case_fold(true);
  ValidateExpected("case_fold",
                   "<DiV><Other xY='AbC' Href='dEf'>Hello</OTHER></diV>",
                   "<div><other xy='AbC' href='dEf'>Hello</other></div>");
}

// Bool that is auto-initialized to false
class Bool {
 public:
  Bool() : value_(false) {}
  Bool(bool value) : value_(value) {}  // Copy constructor // NOLINT
  const bool Test() const { return value_; }

 private:
  bool value_;
};

// Class simply keeps track of which handlers have been called.
class HandlerCalledFilter : public HtmlFilter {
 public:
  HandlerCalledFilter() : enabled_value_(true) {}

  virtual void StartDocument() { called_start_document_ = true; }
  virtual void EndDocument() { called_end_document_ = true;}
  virtual void StartElement(HtmlElement* element) {
    called_start_element_ = true;
  }
  virtual void EndElement(HtmlElement* element) {
    called_end_element_ = true;
  }
  virtual void Cdata(HtmlCdataNode* cdata) { called_cdata_ = true; }
  virtual void Comment(HtmlCommentNode* comment) { called_comment_ = true; }
  virtual void IEDirective(HtmlIEDirectiveNode* directive) {
    called_ie_directive_ = true;
  }
  virtual void Characters(HtmlCharactersNode* characters) {
    called_characters_ = true;
  }
  virtual void Directive(HtmlDirectiveNode* directive) {
    called_directive_ = true;
  }
  virtual void Flush() { called_flush_ = true; }

  virtual void DetermineEnabled(GoogleString* disabled_reason) {
    set_is_enabled(enabled_value_);
  }

  virtual bool CanModifyUrls() { return false; }
  ScriptUsage GetScriptUsage() const override { return kNeverInjectsScripts; }

  void SetEnabled(bool enabled_value) {
    enabled_value_  = enabled_value;
  }
  virtual const char* Name() const { return "HandlerCalled"; }

  Bool called_start_document_;
  Bool called_end_document_;
  Bool called_start_element_;
  Bool called_end_element_;
  Bool called_cdata_;
  Bool called_comment_;
  Bool called_ie_directive_;
  Bool called_characters_;
  Bool called_directive_;
  Bool called_flush_;

 private:
  bool enabled_value_;

  DISALLOW_COPY_AND_ASSIGN(HandlerCalledFilter);
};

class HandlerCalledTest : public HtmlParseTest {
 protected:
  HandlerCalledTest() {
    html_parse_.AddFilter(&handler_called_filter_);
    first_event_listener_ = new HandlerCalledFilter();
    second_event_listener_ = new HandlerCalledFilter();
    html_parse_.add_event_listener(first_event_listener_);
    html_parse_.add_event_listener(second_event_listener_);
  }

  HandlerCalledFilter handler_called_filter_;
  HandlerCalledFilter* first_event_listener_;
  HandlerCalledFilter* second_event_listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HandlerCalledTest);
};

// Check that StartDocument and EndDocument were called for filters.
TEST_F(HandlerCalledTest, StartEndDocumentCalled) {
  Parse("start_end_document_called", "");
  EXPECT_TRUE(handler_called_filter_.called_start_document_.Test());
  EXPECT_TRUE(handler_called_filter_.called_end_document_.Test());
  EXPECT_TRUE(first_event_listener_->called_start_document_.Test());
  EXPECT_TRUE(first_event_listener_->called_end_document_.Test());
  EXPECT_TRUE(second_event_listener_->called_start_document_.Test());
  EXPECT_TRUE(second_event_listener_->called_end_document_.Test());
}

// Check that StartDocument and EndDocument were called for filters.
TEST_F(HandlerCalledTest, StartEndDocumentWithFilterDisabled) {
  handler_called_filter_.SetEnabled(false);
  Parse("start_end_document_called", "");
  EXPECT_FALSE(handler_called_filter_.called_start_document_.Test());
  EXPECT_FALSE(handler_called_filter_.called_end_document_.Test());
  EXPECT_TRUE(first_event_listener_->called_start_document_.Test());
  EXPECT_TRUE(first_event_listener_->called_end_document_.Test());
  EXPECT_TRUE(second_event_listener_->called_start_document_.Test());
  EXPECT_TRUE(second_event_listener_->called_end_document_.Test());

  handler_called_filter_.SetEnabled(true);
  Parse("start_end_document_called", "");
  EXPECT_TRUE(handler_called_filter_.called_start_document_.Test());
  EXPECT_TRUE(handler_called_filter_.called_end_document_.Test());
  EXPECT_TRUE(first_event_listener_->called_start_document_.Test());
  EXPECT_TRUE(first_event_listener_->called_end_document_.Test());
  EXPECT_TRUE(second_event_listener_->called_start_document_.Test());
  EXPECT_TRUE(second_event_listener_->called_end_document_.Test());
}

TEST_F(HandlerCalledTest, StartEndElementCalled) {
  Parse("start_end_element_called", "<p>...</p>");
  EXPECT_TRUE(handler_called_filter_.called_start_element_.Test());
  EXPECT_TRUE(handler_called_filter_.called_end_element_.Test());
  EXPECT_TRUE(first_event_listener_->called_start_element_.Test());
  EXPECT_TRUE(first_event_listener_->called_end_element_.Test());
  EXPECT_TRUE(second_event_listener_->called_start_element_.Test());
  EXPECT_TRUE(second_event_listener_->called_end_element_.Test());
}

TEST_F(HandlerCalledTest, CdataCalled) {
  Parse("cdata_called", "<![CDATA[...]]>");
  // Looks like a directive, but isn't.
  EXPECT_FALSE(handler_called_filter_.called_directive_.Test());
  EXPECT_TRUE(handler_called_filter_.called_cdata_.Test());
  EXPECT_FALSE(first_event_listener_->called_directive_.Test());
  EXPECT_TRUE(first_event_listener_->called_cdata_.Test());
  EXPECT_FALSE(second_event_listener_->called_directive_.Test());
  EXPECT_TRUE(second_event_listener_->called_cdata_.Test());
}

TEST_F(HandlerCalledTest, CommentCalled) {
  Parse("comment_called", "<!--...-->");
  EXPECT_TRUE(handler_called_filter_.called_comment_.Test());
  EXPECT_TRUE(first_event_listener_->called_comment_.Test());
  EXPECT_TRUE(second_event_listener_->called_comment_.Test());
}

TEST_F(HandlerCalledTest, IEDirectiveCalled1) {
  Parse("ie_directive_called", "<!--[if IE]>...<![endif]-->");
  // Looks like a comment, but isn't.
  EXPECT_FALSE(handler_called_filter_.called_comment_.Test());
  EXPECT_TRUE(handler_called_filter_.called_ie_directive_.Test());
  EXPECT_FALSE(first_event_listener_->called_comment_.Test());
  EXPECT_TRUE(first_event_listener_->called_ie_directive_.Test());
  EXPECT_FALSE(second_event_listener_->called_comment_.Test());
  EXPECT_TRUE(second_event_listener_->called_ie_directive_.Test());
}

TEST_F(HandlerCalledTest, IEDirectiveCalled2) {
  // See http://github.com/apache/incubator-pagespeed-mod/issues/136 and
  // http://msdn.microsoft.com/en-us/library/ms537512(VS.85).aspx#dlrevealed
  Parse("ie_directive_called", "<!--[if lte IE 8]>...<![endif]-->");
  EXPECT_FALSE(handler_called_filter_.called_comment_.Test());
  EXPECT_TRUE(handler_called_filter_.called_ie_directive_.Test());
  EXPECT_FALSE(first_event_listener_->called_comment_.Test());
  EXPECT_TRUE(first_event_listener_->called_ie_directive_.Test());
  EXPECT_FALSE(second_event_listener_->called_comment_.Test());
  EXPECT_TRUE(second_event_listener_->called_ie_directive_.Test());
}

TEST_F(HandlerCalledTest, IEDirectiveCalled3) {
  Parse("ie_directive_called", "<!--[if false]>...<![endif]-->");
  EXPECT_FALSE(handler_called_filter_.called_comment_.Test());
  EXPECT_TRUE(handler_called_filter_.called_ie_directive_.Test());
  EXPECT_FALSE(first_event_listener_->called_comment_.Test());
  EXPECT_TRUE(first_event_listener_->called_ie_directive_.Test());
  EXPECT_FALSE(second_event_listener_->called_comment_.Test());
  EXPECT_TRUE(second_event_listener_->called_ie_directive_.Test());
}

// Downlevel-revealed commments normally look like <![if foo]>...<![endif]>.
// However, although most (non-IE) browsers will ignore those, they're
// technically not valid, so some sites use the below trick (which is valid
// HTML, and still works for IE).  For an explanation, see
// http://en.wikipedia.org/wiki/Conditional_comment#
// Downlevel-revealed_conditional_comment
TEST_F(HandlerCalledTest, IEDirectiveCalledRevealedOpen) {
  Parse("ie_directive_called", "<!--[if !IE]><!-->");
  EXPECT_FALSE(handler_called_filter_.called_comment_.Test());
  EXPECT_TRUE(handler_called_filter_.called_ie_directive_.Test());
  EXPECT_FALSE(first_event_listener_->called_comment_.Test());
  EXPECT_TRUE(first_event_listener_->called_ie_directive_.Test());
  EXPECT_FALSE(second_event_listener_->called_comment_.Test());
  EXPECT_TRUE(second_event_listener_->called_ie_directive_.Test());
}
TEST_F(HandlerCalledTest, IEDirectiveCalledRevealedClose) {
  Parse("ie_directive_called", "<!--<![endif]-->");
  EXPECT_FALSE(handler_called_filter_.called_comment_.Test());
  EXPECT_TRUE(handler_called_filter_.called_ie_directive_.Test());
  EXPECT_FALSE(first_event_listener_->called_comment_.Test());
  EXPECT_TRUE(first_event_listener_->called_ie_directive_.Test());
  EXPECT_FALSE(second_event_listener_->called_comment_.Test());
  EXPECT_TRUE(second_event_listener_->called_ie_directive_.Test());
}

// Unit tests for event-list manipulation.  In these tests, we do not parse
// HTML input text, but instead create two 'Characters' nodes and use the
// event-list manipulation methods and make sure they render as expected.
class EventListManipulationTest : public HtmlParseTest {
 protected:
  EventListManipulationTest() { }

  virtual void SetUp() {
    HtmlParseTest::SetUp();
    static const char kUrl[] = "http://html.parse.test/event_list_test.html";
    ASSERT_TRUE(html_parse_.StartParse(kUrl));
    node1_ = html_parse_.NewCharactersNode(NULL, "1");
    HtmlTestingPeer::AddEvent(&html_parse_,
                              new HtmlCharactersEvent(node1_, -1));
    node2_ = html_parse_.NewCharactersNode(NULL, "2");
    node3_ = html_parse_.NewCharactersNode(NULL, "3");
    // Note: the last 2 are not added in SetUp.
  }

  virtual void TearDown() {
    html_parse_.FinishParse();
    HtmlParseTest::TearDown();
  }

  void CheckExpected(const GoogleString& expected) {
    SetupWriter();
    html_parse()->ApplyFilter(html_writer_filter_.get());
    EXPECT_EQ(expected, output_buffer_);
  }

  HtmlCharactersNode* node1_;
  HtmlCharactersNode* node2_;
  HtmlCharactersNode* node3_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventListManipulationTest);
};

TEST_F(EventListManipulationTest, TestReplace) {
  EXPECT_TRUE(html_parse_.ReplaceNode(node1_, node2_));
  CheckExpected("2");
}

TEST_F(EventListManipulationTest, TestInsertNodeBeforeNode) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  html_parse_.InsertNodeBeforeNode(node1_, node2_);
  CheckExpected("21");
  html_parse_.InsertNodeBeforeNode(node1_, node3_);
  CheckExpected("231");
}

TEST_F(EventListManipulationTest, TestInsertNodeAfterNode) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  html_parse_.InsertNodeAfterNode(node1_, node2_);
  CheckExpected("12");
  html_parse_.InsertNodeAfterNode(node1_, node3_);
  CheckExpected("132");
}

TEST_F(EventListManipulationTest, TestInsertNodeBeforeCurrent) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  html_parse_.InsertNodeBeforeCurrent(node2_);
  // Current is left at queue_.end() after the AddEvent.
  CheckExpected("12");

  HtmlTestingPeer::SetCurrent(&html_parse_, node1_);
  html_parse_.InsertNodeBeforeCurrent(node3_);
  CheckExpected("312");
}

TEST_F(EventListManipulationTest, TestInsertNodeAfterCurrent) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::SetCurrent(&html_parse_, node1_);
  html_parse_.InsertNodeAfterCurrent(node2_);
  // Note that if we call CheckExpected here it will mutate current_.
  html_parse_.InsertNodeAfterCurrent(node3_);
  CheckExpected("123");
}

TEST_F(EventListManipulationTest, TestDeleteOnly) {
  html_parse_.DeleteNode(node1_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestDeleteFirst) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  html_parse_.DeleteNode(node1_);
  CheckExpected("23");
  html_parse_.DeleteNode(node2_);
  CheckExpected("3");
  html_parse_.DeleteNode(node3_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestDeleteLast) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  html_parse_.DeleteNode(node3_);
  CheckExpected("12");
  html_parse_.DeleteNode(node2_);
  CheckExpected("1");
  html_parse_.DeleteNode(node1_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestDeleteMiddle) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  html_parse_.DeleteNode(node2_);
  CheckExpected("13");
}

// Note that an unconditionally sanity check runs after every
// filter, verifying that all the parent-pointers are correct.
// CheckExpected applies the HtmlWriterFilter, so it runs the
// parent-pointer check.
TEST_F(EventListManipulationTest, TestAddParentToSequence) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node3_, div));
  CheckExpected("<div>123</div>");

  // Now interpose a span between the div and the Characters nodes.
  HtmlElement* span = html_parse_.NewElement(div, HtmlName::kSpan);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node2_, span));
  CheckExpected("<div><span>12</span>3</div>");

  // Next, add an HTML block above the div.  Note that we pass 'div' in as
  // both 'first' and 'last'.
  HtmlElement* html = html_parse_.NewElement(NULL, HtmlName::kHtml);
  EXPECT_TRUE(html_parse_.AddParentToSequence(div, div, html));
  CheckExpected("<html><div><span>12</span>3</div></html>");
}

TEST_F(EventListManipulationTest, TestPrependChild) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.InsertNodeBeforeCurrent(div);
  CheckExpected("1<div></div>");

  html_parse_.PrependChild(div, node2_);
  CheckExpected("1<div>2</div>");
  html_parse_.PrependChild(div, node3_);
  CheckExpected("1<div>32</div>");

  // TODO(sligocki): Test with elements that don't explicitly end like image.
}

TEST_F(EventListManipulationTest, TestAppendChild) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.InsertNodeBeforeCurrent(div);
  CheckExpected("1<div></div>");

  html_parse_.AppendChild(div, node2_);
  CheckExpected("1<div>2</div>");
  html_parse_.AppendChild(div, node3_);
  CheckExpected("1<div>23</div>");

  // TODO(sligocki): Test with elements that don't explicitly end like image.
}

TEST_F(EventListManipulationTest, TestAddParentToSequenceDifferentParents) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node2_, div));
  CheckExpected("<div>12</div>");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("<div>12</div>3");
  EXPECT_FALSE(html_parse_.AddParentToSequence(node2_, node3_, div));
}

TEST_F(EventListManipulationTest, TestDeleteGroup) {
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node2_, div));
  CheckExpected("<div>12</div>");
  html_parse_.DeleteNode(div);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestMoveElementIntoParent1) {
  HtmlElement* head = html_parse_.NewElement(NULL, HtmlName::kHead);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node1_, head));
  CheckExpected("<head>1</head>");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node2_, node2_, div));
  CheckExpected("<head>1</head><div>2</div>");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("<head>1</head><div>2</div>3");
  HtmlTestingPeer::SetCurrent(&html_parse_, div);
  EXPECT_TRUE(html_parse_.MoveCurrentInto(head));
  CheckExpected("<head>1<div>2</div></head>3");
}

TEST_F(EventListManipulationTest, TestMoveElementIntoParent2) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlElement* head = html_parse_.NewElement(NULL, HtmlName::kHead);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node1_, head));
  CheckExpected("<head>1</head>");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("<head>1</head>23");
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node3_, node3_, div));
  CheckExpected("<head>1</head>2<div>3</div>");
  HtmlTestingPeer::SetCurrent(&html_parse_, div);
  EXPECT_TRUE(html_parse_.MoveCurrentInto(head));
  CheckExpected("<head>1<div>3</div></head>2");
  EXPECT_TRUE(html_parse_.DeleteSavingChildren(div));
  CheckExpected("<head>13</head>2");
  EXPECT_TRUE(html_parse_.DeleteSavingChildren(head));
  CheckExpected("132");
}

TEST_F(EventListManipulationTest, TestDeleteSavingChildrenEnd) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node1_, div));
  CheckExpected("<div>1</div>");
  EXPECT_TRUE(html_parse_.DeleteSavingChildren(div));
  CheckExpected("1");
}

TEST_F(EventListManipulationTest, TestMoveCurrentBefore) {
  // Setup events.
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node2_, div));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("<div>12</div>3");
  HtmlTestingPeer::SetCurrent(&html_parse_, node3_);

  // Test MoveCurrentBefore().
  EXPECT_TRUE(html_parse_.MoveCurrentBefore(node2_));
  CheckExpected("<div>132</div>");

#ifdef NDEBUG
  // Test that current_ pointing to end() does not crash in non-debug build.
  // In debug build, there is a LOG(DFATAL), so we cannot run this.
  // NOTE: We do not expect this case ever to happen in normal code.
  EXPECT_FALSE(html_parse_.MoveCurrentBefore(node2_));
  CheckExpected("<div>132</div>");
#endif

  // Test that current_ pointing to a containing object will not work.
  HtmlElement* span = html_parse_.NewElement(NULL, HtmlName::kSpan);
  EXPECT_TRUE(html_parse_.AddParentToSequence(div, div, span));
  CheckExpected("<span><div>132</div></span>");
  HtmlTestingPeer::SetCurrent(&html_parse_, span);

  EXPECT_FALSE(html_parse_.MoveCurrentBefore(node2_));
  CheckExpected("<span><div>132</div></span>");
}

TEST_F(EventListManipulationTest, TestCoalesceOnAdd) {
  CheckExpected("1");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  CheckExpected("12");

  // this will coalesce node1 and node2 togethers.  So there is only
  // one node1_="12", and node2_ is gone.  Deleting node1_ will now
  // leave us empty
  html_parse_.DeleteNode(node1_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestCoalesceOnDelete) {
  CheckExpected("1");
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.AddElement(div, -1);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer testing_peer;
  testing_peer.SetNodeParent(node2_, div);
  html_parse_.CloseElement(div, HtmlElement::EXPLICIT_CLOSE, -1);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("1<div>2</div>3");

  // Removing the div, leaving the children intact...
  EXPECT_TRUE(html_parse_.DeleteSavingChildren(div));
  CheckExpected("123");

  // At this point, node1, node2, and node3 are automatically coalesced.
  // This means when we remove node1, all the content will disappear.
  html_parse_.DeleteNode(node1_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestHasChildren) {
  CheckExpected("1");
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.AddElement(div, -1);
  EXPECT_FALSE(html_parse_.HasChildrenInFlushWindow(div));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer testing_peer;
  testing_peer.SetNodeParent(node2_, div);

  // Despite having added a new element into the stream, the div is not
  // closed yet, so it's not recognized as a child.
  EXPECT_FALSE(html_parse_.HasChildrenInFlushWindow(div));

  html_parse_.CloseElement(div, HtmlElement::EXPLICIT_CLOSE, -1);
  EXPECT_TRUE(html_parse_.HasChildrenInFlushWindow(div));
  EXPECT_TRUE(html_parse_.DeleteNode(node2_));
  EXPECT_FALSE(html_parse_.HasChildrenInFlushWindow(div));
}

TEST_F(EventListManipulationTest, AppendComment) {
  html_parse_.InsertComment("hello");
  CheckExpected("1<!--hello-->");
}

TEST_F(EventListManipulationTest, AppendCommentWithEscaping) {
  html_parse_.InsertComment("<i>hello</i> <!--world-->");
  CheckExpected("1<!--&lt;i&gt;hello&lt;/i&gt; &lt;!--world--&gt;-->");
}

TEST_F(EventListManipulationTest, CommentBeforeDiv1) {
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.AddElement(div, -1);
  html_parse_.InsertComment("hello");
  html_parse_.CloseElement(div, HtmlElement::EXPLICIT_CLOSE, -1);
  CheckExpected("1<!--hello--><div></div>");
}

TEST_F(EventListManipulationTest, CommentBeforeDiv2) {
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.InsertComment("hello");
  html_parse_.AddElement(div, -1);
  html_parse_.CloseElement(div, HtmlElement::EXPLICIT_CLOSE, -1);
  CheckExpected("1<!--hello--><div></div>");
}

TEST_F(EventListManipulationTest, CommentAfterDiv) {
  HtmlElement* div = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.AddElement(div, -1);
  html_parse_.CloseElement(div, HtmlElement::EXPLICIT_CLOSE, -1);
  html_parse_.InsertComment("hello");
  CheckExpected("1<div></div><!--hello-->");
}

TEST_F(EventListManipulationTest, CommentAfterFirstDiv) {
  HtmlElement* div1 = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.AddElement(div1, -1);
  html_parse_.CloseElement(div1, HtmlElement::EXPLICIT_CLOSE, -1);
  HtmlElement* div2 = html_parse_.NewElement(NULL, HtmlName::kDiv);
  html_parse_.AddElement(div2, -1);
  html_parse_.CloseElement(div2, HtmlElement::EXPLICIT_CLOSE, -1);
  HtmlTestingPeer::SetCurrent(&html_parse_, div1);
  html_parse_.InsertComment("hello");
  CheckExpected("1<div></div><!--hello--><div></div>");
}

class InsertCommentOnFirstDivFilter : public EmptyHtmlFilter {
 public:
  InsertCommentOnFirstDivFilter(bool at_start, HtmlParse* parse)
      : html_parse_(parse),
        at_start_(at_start),
        first_(true) {
  }

  virtual void StartDocument() { first_ = true; }
  virtual void StartElement(HtmlElement* element) { Insert(true, element); }
  virtual void EndElement(HtmlElement* element) { Insert(false, element); }
  virtual const char* Name() const { return "InsertCommentOnFirstDivFilter"; }

 private:
  void Insert(bool at_start, HtmlElement* element) {
    if (first_ && (at_start == at_start_) &&
        (element->keyword() == HtmlName::kDiv)) {
      html_parse_->InsertComment("hello");
      first_ = false;
    }
  }


 private:
  HtmlParse* html_parse_;
  bool at_start_;
  bool first_;

  DISALLOW_COPY_AND_ASSIGN(InsertCommentOnFirstDivFilter);
};

TEST_F(HtmlParseTestNoBody, CommentInsideFirstDiv) {
  InsertCommentOnFirstDivFilter insert_at_first_div(true, &html_parse_);
  html_parse_.AddFilter(&insert_at_first_div);
  SetupWriter();
  ValidateExpected("comment_inside_first_div",
                   "1<div>2</div>3<div>4</div>5",
                   "1<!--hello--><div>2</div>3<div>4</div>5");
}

TEST_F(HtmlParseTestNoBody, CommentAfterFirstDiv) {
  InsertCommentOnFirstDivFilter insert_at_first_div(false, &html_parse_);
  html_parse_.AddFilter(&insert_at_first_div);
  SetupWriter();
  ValidateExpected("comment_inside_first_div",
                   "1<div>2</div>3<div>4</div>5",
                   "1<div>2</div><!--hello-->3<div>4</div>5");
}

TEST_F(HtmlParseTestNoBody, InsertCommentFromEmpty) {
  html_parse_.InsertComment("hello");
  SetupWriter();
  html_parse()->ApplyFilter(html_writer_filter_.get());
  EXPECT_EQ("<!--hello-->", output_buffer_);
}

TEST_F(HtmlParseTestNoBody, InsertCommentFromFlushInLargeCharactersBlock) {
  SetupWriter();
  html_parse_.StartParse("http://test.com/blank_flush.html");
  html_parse_.ParseText("<style>bytes:");
  // This should be inserted before <style>.
  EXPECT_TRUE(html_parse_.InsertComment("FLUSH1"));
  html_parse_.Flush();
  html_parse_.ParseText(":more:");
  html_parse_.Flush();
  html_parse_.ParseText(":still more:");
  // We are inside a literal block, so it's not safe to insert a comment here.
  // This should not show up in output_buffer_.
  EXPECT_FALSE(html_parse_.InsertComment("FLUSH2"));
  html_parse_.Flush();
  html_parse_.ParseText(":final bytes:</style>");
  EXPECT_TRUE(html_parse_.InsertComment("FLUSH3"));
  html_parse_.FinishParse();

  EXPECT_EQ("<!--FLUSH1--><style>bytes::more::still more::final bytes:</style>"
            "<!--FLUSH3-->",
            output_buffer_);
}

TEST_F(HtmlParseTestNoBody, InsertCommentFromFlushInEmptyCharactersBlock) {
  SetupWriter();
  html_parse_.StartParse("http://test.com/blank_flush.html");
  html_parse_.ParseText("<style>");
  // This should be inserted before <style>.
  EXPECT_TRUE(html_parse_.InsertComment("FLUSH1"));
  EXPECT_TRUE(html_parse_.InsertComment("FLUSH2"));
  html_parse_.Flush();
  html_parse_.ParseText("</style>");
  EXPECT_TRUE(html_parse_.InsertComment("FLUSH3"));
  html_parse_.FinishParse();

  EXPECT_EQ("<!--FLUSH1--><!--FLUSH2--><style></style><!--FLUSH3-->",
            output_buffer_);
}

// Unit tests for attribute manipulation.
// Goal is to make sure we don't (eg) read deallocated storage
// while manipulating attribute values.
class AttributeManipulationTest : public HtmlParseTest {
 protected:
  AttributeManipulationTest() { }

  virtual void SetUp() {
    HtmlParseTest::SetUp();
    static const char kUrl[] =
        "http://html.parse.test/attribute_manipulation_test.html";
    ASSERT_TRUE(html_parse_.StartParse(kUrl));
    node_ = html_parse_.NewElement(NULL, HtmlName::kA);
    html_parse_.AddElement(node_, 0);
    html_parse_.AddAttribute(node_, HtmlName::kHref, "http://www.google.com/");
    node_->AddAttribute(html_parse_.MakeName(HtmlName::kId), "37",
                        HtmlElement::NO_QUOTE);
    node_->AddAttribute(html_parse_.MakeName(HtmlName::kClass), "search!",
                        HtmlElement::SINGLE_QUOTE);
    // Add a binary attribute (one without value).
    node_->AddAttribute(html_parse_.MakeName(HtmlName::kSelected),
                        StringPiece(), HtmlElement::NO_QUOTE);
    html_parse_.CloseElement(node_, HtmlElement::BRIEF_CLOSE, 0);
  }

  virtual void TearDown() {
    html_parse_.FinishParse();
    HtmlParseTest::TearDown();
  }

  void CheckExpected(const GoogleString& expected) {
    SetupWriter();
    html_parse_.ApplyFilter(html_writer_filter_.get());
    EXPECT_EQ(expected, output_buffer_);
  }

  int NumAttributes(HtmlElement* element) {
    int size = 0;
    const HtmlElement::AttributeList& attrs = element->attributes();
    for (HtmlElement::AttributeConstIterator i(attrs.begin());
         i != attrs.end(); ++i) {
      ++size;
    }

    return size;
  }

  HtmlElement::Attribute* AttributeAt(HtmlElement* element, int index) {
    int pos = 0;
    HtmlElement::AttributeList* attrs = element->mutable_attributes();
    for (HtmlElement::AttributeIterator i(attrs->begin());
         i != attrs->end(); ++i) {
      if (pos == index) {
        return i.Get();
      }
      ++pos;
    }
    return NULL;
  }

  HtmlElement* node_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AttributeManipulationTest);
};

TEST_F(AttributeManipulationTest, PropertiesAndDeserialize) {
  StringPiece google("http://www.google.com/");
  StringPiece number37("37");
  StringPiece search("search!");
  EXPECT_EQ(4, NumAttributes(node_));
  EXPECT_EQ(google, node_->AttributeValue(HtmlName::kHref));
  EXPECT_EQ(number37, node_->AttributeValue(HtmlName::kId));
  EXPECT_EQ(search, node_->AttributeValue(HtmlName::kClass));
  // Returns NULL for attributes that do not exist ...
  EXPECT_TRUE(NULL == node_->AttributeValue(HtmlName::kNotAKeyword));
  // ... and for attributes which have no value.
  EXPECT_TRUE(NULL == node_->AttributeValue(HtmlName::kSelected));
  // Returns NULL for attributes that do not exist.
  EXPECT_TRUE(NULL == node_->FindAttribute(HtmlName::kNotAKeyword));
  // Returns an attribute reference for attributes without values.
  HtmlElement::Attribute* selected = node_->FindAttribute(HtmlName::kSelected);
  EXPECT_TRUE(NULL != selected);
  EXPECT_TRUE(NULL == selected->DecodedValueOrNull());
  EXPECT_EQ(google, node_->AttributeValue(HtmlName::kHref));
  EXPECT_EQ(number37, node_->AttributeValue(HtmlName::kId));
  EXPECT_EQ(search, node_->AttributeValue(HtmlName::kClass));
  EXPECT_EQ(google, node_->FindAttribute(HtmlName::kHref)->escaped_value());
  EXPECT_EQ(number37, node_->FindAttribute(HtmlName::kId)->escaped_value());
  EXPECT_EQ(search, node_->FindAttribute(HtmlName::kClass)->escaped_value());
  CheckExpected("<a href=\"http://www.google.com/\" id=37 class='search!'"
                " selected />");
}

TEST_F(AttributeManipulationTest, AddAttribute) {
  html_parse_.AddAttribute(node_, HtmlName::kLang, "ENG-US");
  CheckExpected("<a href=\"http://www.google.com/\" id=37 class='search!'"
                " selected lang=\"ENG-US\"/>");
}

TEST_F(AttributeManipulationTest, DeleteAttribute) {
  node_->DeleteAttribute(HtmlName::kId);
  CheckExpected("<a href=\"http://www.google.com/\" class='search!'"
                " selected />");
  node_->DeleteAttribute(HtmlName::kSelected);
  CheckExpected("<a href=\"http://www.google.com/\" class='search!'/>");
}

TEST_F(AttributeManipulationTest, ModifyAttribute) {
  HtmlElement::Attribute* href =
      node_->FindAttribute(HtmlName::kHref);
  EXPECT_TRUE(href != NULL);
  href->SetValue("google");
  href->set_quote_style(HtmlElement::SINGLE_QUOTE);
  html_parse_.SetAttributeName(href, HtmlName::kSrc);
  CheckExpected("<a src='google' id=37 class='search!' selected />");
}

TEST_F(AttributeManipulationTest, ModifyKeepAttribute) {
  HtmlElement::Attribute* href =
      node_->FindAttribute(HtmlName::kHref);
  EXPECT_TRUE(href != NULL);
  // This apparently do-nothing call to SetValue exposed an allocation bug.
  href->SetValue(href->DecodedValueOrNull());
  href->set_quote_style(href->quote_style());
  href->set_name(href->name());
  CheckExpected("<a href=\"http://www.google.com/\" id=37 class='search!'"
                " selected />");
}

TEST_F(AttributeManipulationTest, BadUrl) {
  EXPECT_FALSE(html_parse_.StartParse(")(*&)(*&(*"));

  // To avoid having the TearDown crash, restart the parse.
  html_parse_.StartParse("http://www.example.com");
}

TEST_F(AttributeManipulationTest, CloneElement) {
  HtmlElement* clone = html_parse_.CloneElement(node_);

  // The clone is identical (but not the same object).
  EXPECT_NE(clone, node_);
  EXPECT_EQ(HtmlName::kA, clone->keyword());
  EXPECT_EQ(node_->style(), clone->style());
  EXPECT_EQ(4, NumAttributes(clone));
  EXPECT_EQ(HtmlName::kHref, AttributeAt(clone, 0)->keyword());
  EXPECT_STREQ("http://www.google.com/",
               AttributeAt(clone, 0)->DecodedValueOrNull());
  EXPECT_EQ(HtmlName::kId, AttributeAt(clone, 1)->keyword());
  EXPECT_STREQ("37", AttributeAt(clone, 1)->DecodedValueOrNull());
  EXPECT_EQ(HtmlName::kClass, AttributeAt(clone, 2)->keyword());
  EXPECT_STREQ("search!", AttributeAt(clone, 2)->DecodedValueOrNull());
  EXPECT_EQ(HtmlName::kSelected, AttributeAt(clone, 3)->keyword());
  EXPECT_EQ(NULL, AttributeAt(clone, 3)->DecodedValueOrNull());

  HtmlElement::Attribute* id = clone->FindAttribute(HtmlName::kId);
  ASSERT_TRUE(id != NULL);
  id->SetValue("38");

  // Clone is not added initially, and the original is not touched.
  CheckExpected("<a href=\"http://www.google.com/\" id=37 class='search!'"
                " selected />");

  // Looks sane when added.
  html_parse_.InsertNodeBeforeNode(node_, clone);
  CheckExpected("<a href=\"http://www.google.com/\" id=38 class='search!'"
                " selected />"
                "<a href=\"http://www.google.com/\" id=37 class='search!'"
                " selected />");
}

TEST_F(HtmlParseTest, NoDisabledFilter) {
  std::vector<GoogleString> disabled_filters;
  ASSERT_TRUE(disabled_filters.empty());

  html_parse_.SetDynamicallyDisabledFilterList(&disabled_filters);

  DisableTestFilter filter("not_disabled_filter", true, "Ignored reason");
  html_parse_.AddFilter(&filter);

  Parse("not_disabled_filter", "<!-- Empty body -->");

  EXPECT_TRUE(disabled_filters.empty());
}

TEST_F(HtmlParseTest, DisabledFilters) {
  std::vector<GoogleString> disabled_filters;
  ASSERT_TRUE(disabled_filters.empty());

  html_parse_.SetDynamicallyDisabledFilterList(&disabled_filters);

  DisableTestFilter filter1("not_disabled_filter1", true, "Ignored reason");
  html_parse_.AddFilter(&filter1);

  DisableTestFilter disabled_filter1("disabled_filter1", false, "");
  html_parse_.AddFilter(&disabled_filter1);

  DisableTestFilter filter2("not_disabled_filter2", true, "Ignored reason");
  html_parse_.AddFilter(&filter2);

  DisableTestFilter disabled_filter2("disabled_filter2", false, "");
  html_parse_.AddFilter(&disabled_filter2);

  DisableTestFilter filter3("not_disabled_filter3", true, "Ignored reason");
  html_parse_.AddFilter(&filter3);

  Parse("disabled_filter", "<!-- Empty body -->");

  EXPECT_THAT(disabled_filters,
              UnorderedElementsAre(disabled_filter1.ExpectedDisabledMessage(),
                                   disabled_filter2.ExpectedDisabledMessage()));
}

TEST_F(HtmlParseTest, DisabledFilterWithReason) {
  std::vector<GoogleString> disabled_filters;
  ASSERT_TRUE(disabled_filters.empty());
  html_parse_.SetDynamicallyDisabledFilterList(&disabled_filters);

  const GoogleString disabled_reason("Some reason");
  DisableTestFilter filter("disabled_filter_with_reason", false,
                           disabled_reason);
  html_parse_.AddFilter(&filter);

  Parse("disabled_filter_with_reason", "<!-- Empty body -->");

  EXPECT_THAT(disabled_filters,
              UnorderedElementsAre(filter.ExpectedDisabledMessage()));
}

class DisableFilterOnBody : public EmptyHtmlFilter {
 public:
  DisableFilterOnBody(HtmlFilter* filter_to_disable,
                      HtmlParse* parse)
      : filter_to_disable_(filter_to_disable),
        html_parse_(parse) {
  }

  virtual void StartElement(HtmlElement* element) {
    if (element->keyword() == HtmlName::kBody) {
      filter_to_disable_->set_is_enabled(false);
      HtmlTestingPeer::set_buffer_events(html_parse_, false);
    }
  }
  virtual const char* Name() const { return "DisableFilterOnBody"; }

 private:
  HtmlFilter* filter_to_disable_;
  HtmlParse* html_parse_;
};

class CountingCallbacksFilter : public EmptyHtmlFilter {
 public:
  CountingCallbacksFilter()
      : num_start_elements_(0),
        num_end_elements_(0),
        num_char_elements_(0) {
  }
  int num_start_elements() const { return num_start_elements_; }
  int num_end_elements() const { return num_end_elements_; }
  int num_char_elements() const { return num_char_elements_; }

 protected:
  virtual void StartDocument() {
    num_start_elements_ = 0;
    num_end_elements_ = 0;
    num_char_elements_ = 0;
  }

  virtual void StartElement(HtmlElement* element) {
    ++num_start_elements_;
  }

  virtual void EndElement(HtmlElement* element) {
    ++num_end_elements_;
  }

  virtual void Characters(HtmlCharactersNode* characters) {
    ++num_char_elements_;
  }

  virtual const char* Name() const { return "CountingCallbacksFilter"; }

 private:
  int num_start_elements_;
  int num_end_elements_;
  int num_char_elements_;

  DISALLOW_COPY_AND_ASSIGN(CountingCallbacksFilter);
};

TEST_F(HtmlParseTest, BufferEventsOnEventListener) {
  CountingCallbacksFilter counter_that_stays_enabled;
  CountingCallbacksFilter counter_to_disable;
  html_parse_.AddFilter(&counter_that_stays_enabled);
  html_parse_.AddFilter(&counter_to_disable);
  html_parse_.add_event_listener(new DisableFilterOnBody(
      &counter_to_disable, &html_parse_));
  static const char kInput[] =
      "<html><head><title>foo</title><body>hello, world</body></html>";
  const StringPiece kInputPiece(kInput, STATIC_STRLEN(kInput));
  for (int i = 0, n = STATIC_STRLEN(kInput); i < n; ++i) {
    counter_to_disable.set_is_enabled(true);
    html_parse_.StartParse(StringPrintf("http://example.com/doc_%d.html", i));
    HtmlTestingPeer::set_buffer_events(&html_parse_, true);
    html_parse_.ParseText(kInputPiece.substr(0, i));
    html_parse_.Flush();
    html_parse_.ParseText(kInputPiece.substr(i));
    html_parse_.FinishParse();
    EXPECT_EQ(0, counter_to_disable.num_start_elements());
    EXPECT_EQ(0, counter_to_disable.num_end_elements());
    EXPECT_EQ(0, counter_to_disable.num_char_elements());
    EXPECT_EQ(4, counter_that_stays_enabled.num_start_elements());
    EXPECT_EQ(4, counter_that_stays_enabled.num_end_elements());
    EXPECT_EQ(2, counter_that_stays_enabled.num_char_elements());
  }
}

// Checks that deleting nodes does not change the expected order of
// HTML parse events. We delete any node of del_node_type_, but we
// only delete it when we see a tag of type del_from_type_ (and
// del_from_start_tag indicates whether we do it when we see the start
// tag or the end tag of del_from_type). Can be configured to remove
// nodes using DeleteSavingChildren, DeleteNode, or
// MakeElementInvisible.
class DeleteNodesFilter : public CountingCallbacksFilter {
 public:
  explicit DeleteNodesFilter(HtmlParse* html_parse)
      : html_parse_(html_parse),
        delete_node_type_(HtmlName::kNotAKeyword),
        delete_from_type_(HtmlName::kNotAKeyword),
        delete_on_open_tag_(false),
        save_children_(true),
        make_invisible_(false),
        num_deleted_elements_(0),
        flushes_preventing_delete_(0) {
  }

  void set_delete_node_type(HtmlName::Keyword keyword) {
    delete_node_type_ = keyword;
  }

  void set_save_children(bool x) { save_children_ = x; }
  void set_make_invisible(bool x) { make_invisible_ = x; }

  void set_delete_from_type(HtmlName::Keyword keyword) {
    delete_from_type_ = keyword;
  }

  void set_delete_on_open_tag(bool del_from_start) {
    delete_on_open_tag_ = del_from_start;
  }

  int num_deleted_elements() const { return num_deleted_elements_; }
  int flushes_preventing_delete() const { return flushes_preventing_delete_; }

 protected:
  virtual void StartDocument() {
    pending_deletes_.clear();
    num_deleted_elements_ = 0;
    flushes_preventing_delete_ = 0;
    // Note: we do not clear save_children_ or make_invisible_ here because
    // we re-use these settings when repeating tests with different flush
    // windows.
  }

  virtual void StartElement(HtmlElement* element) {
    CountingCallbacksFilter::StartElement(element);
    if (element->keyword() == delete_node_type_) {
      pending_deletes_.push_back(element);
    }
    if (delete_on_open_tag_ && element->keyword() == delete_from_type_) {
      DeleteElements();
    }
  }

  virtual void EndElement(HtmlElement* element) {
    CountingCallbacksFilter::EndElement(element);
    if (!delete_on_open_tag_ && element->keyword() == delete_from_type_) {
      DeleteElements();
    }
  }

  virtual void Flush() {
    // We can't delete an element that has been flushed.
    for (int i = 0, n = pending_deletes_.size(); i < n; ++i) {
      ++flushes_preventing_delete_;
    }
    pending_deletes_.clear();
  }

  virtual const char* Name() const { return "DeleteNodesFilter"; }

 private:
  void DeleteElements() {
    for (int i = 0, n = pending_deletes_.size(); i < n; ++i) {
      HtmlElement* element = pending_deletes_[i];
      bool success = make_invisible_
          ? html_parse_->MakeElementInvisible(element)
          : (save_children_
             ? html_parse_->DeleteSavingChildren(element)
             : html_parse_->DeleteNode(element));
      if (success) {
        ++num_deleted_elements_;
      }
    }
    pending_deletes_.clear();
  }

  HtmlParse* html_parse_;
  std::vector<HtmlElement*> pending_deletes_;
  HtmlName::Keyword delete_node_type_;
  HtmlName::Keyword delete_from_type_;
  bool delete_on_open_tag_;
  bool save_children_;
  bool make_invisible_;
  int num_deleted_elements_;
  int flushes_preventing_delete_;

  DISALLOW_COPY_AND_ASSIGN(DeleteNodesFilter);
};

class HtmlParseDeleteTest : public HtmlParseTest {
 protected:
  HtmlParseDeleteTest()
      : delete_filter_(html_parse()),
        total_successes_(0),
        total_failures_(0) {
    html_parse()->AddFilter(&delete_filter_);
    SetupWriter();
  }

  void DeleteTest(StringPiece input,
                  StringPiece expected_output_if_deletes_worked) {
    for (int i = 0, n = input.size(); i < n; ++i) {
      ParseWithFlush(input, i);
      if (delete_filter_.num_deleted_elements() != 0) {
        EXPECT_STREQ(expected_output_if_deletes_worked,
                     output_buffer_) << " flush " << i;
        ++total_successes_;
      } else {
        EXPECT_STREQ(input, output_buffer_) << " flush " << i;
        ++total_failures_;
      }
      output_buffer_.clear();
    }
  }

  DeleteNodesFilter delete_filter_;
  int total_successes_;
  int total_failures_;
};

TEST_F(HtmlParseDeleteTest, DeleteAtStartAcrossFlush) {
  delete_filter_.set_delete_on_open_tag(true);
  delete_filter_.set_save_children(false);
  delete_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_filter_.set_delete_from_type(HtmlName::kDiv);
  const StringPiece kInput("1<div id=a>hello</div>2");
  DeleteTest(kInput, "12");

  // We can utilize the infrastructure in DeferCurrentNode to make it
  // possible to delete nodes from their StartElement even if their
  // EndElement is not in the flush window
  EXPECT_EQ(0, total_failures_);

  // If the both the StartElement and EndElement are visible, then
  // we should successfully eliminate the div and its contents.  That
  // will happen every time.
  EXPECT_EQ(kInput.size(), total_successes_);
}

TEST_F(HtmlParseDeleteTest, DeleteAtEndAcrossFlush) {
  delete_filter_.set_delete_on_open_tag(false);
  delete_filter_.set_save_children(false);
  delete_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_filter_.set_delete_from_type(HtmlName::kDiv);
  DeleteTest("1<div id=a>hello</div>2", "12");

  // If the flush happened in the middle of the div, then we will
  // fail.  That will happen at least sometimes.
  EXPECT_LT(0, total_failures_);

  // If the both the StartElement and EndElement are visible, then
  // we should successfully eliminate the div and its contents.  That
  // will happen at least sometimes.
  EXPECT_LT(0, total_successes_);
}

TEST_F(HtmlParseDeleteTest, InvisibleAtStart) {
  delete_filter_.set_delete_on_open_tag(true);
  delete_filter_.set_make_invisible(true);
  delete_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_filter_.set_delete_from_type(HtmlName::kDiv);
  const StringPiece kInput("1<div id=a>hello</div>2");
  DeleteTest(kInput, "1hello2");

  // It is always possible to make nodes invisible as long as their
  // StartElement has not been flushed.
  EXPECT_EQ(0, total_failures_);
  EXPECT_EQ(kInput.size(), total_successes_);
}

TEST_F(HtmlParseDeleteTest, InvisibleAtEnd) {
  delete_filter_.set_delete_on_open_tag(false);
  delete_filter_.set_make_invisible(true);
  delete_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_filter_.set_delete_from_type(HtmlName::kDiv);
  DeleteTest("1<div id=a>hello</div>2", "1hello2");

  // If the flush happened in the middle of the div, then we will
  // fail.  That will happen at least sometimes.
  EXPECT_LT(0, total_failures_);

  // If the both the StartElement and EndElement are visible, then
  // we should successfully eliminate the div and its contents.  That
  // will happen at least sometimes.
  EXPECT_LT(0, total_successes_);
}

class EventListOrderTest : public HtmlParseTest {
 protected:
  EventListOrderTest()
      : delete_nodes_filter_(&html_parse_) {
    html_parse_.AddFilter(&delete_nodes_filter_);
  }

  virtual bool AddBody() const { return false; }
  virtual bool AddHtmlTags() const { return false; }

  DeleteNodesFilter delete_nodes_filter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventListOrderTest);
};

TEST_F(EventListOrderTest, DeleteSavingChildrenCalledOnOpen) {
  delete_nodes_filter_.set_delete_on_open_tag(true);
  delete_nodes_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_nodes_filter_.set_delete_from_type(HtmlName::kDiv);
  ValidateExpected("delete_saving_children_open",
                   "<div><p>1</p></div><span>2</span>",
                   "<p>1</p><span>2</span>");
  EXPECT_EQ(delete_nodes_filter_.num_start_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_end_elements(), 2);
  EXPECT_EQ(delete_nodes_filter_.num_char_elements(), 2);
  EXPECT_EQ(delete_nodes_filter_.num_deleted_elements(), 1);
}

TEST_F(EventListOrderTest, DeleteSavingChildrenCalledOnClose) {
  delete_nodes_filter_.set_delete_on_open_tag(false);
  delete_nodes_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_nodes_filter_.set_delete_from_type(HtmlName::kDiv);
  ValidateExpected("delete_saving_children_close",
                   "<div><p>1</p></div><span>2</span>",
                   "<p>1</p><span>2</span>");
  EXPECT_EQ(delete_nodes_filter_.num_start_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_end_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_char_elements(), 2);
  EXPECT_EQ(delete_nodes_filter_.num_deleted_elements(), 1);
}

TEST_F(EventListOrderTest, DeleteSavingChildrenCalledInner) {
  delete_nodes_filter_.set_delete_on_open_tag(true);
  delete_nodes_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_nodes_filter_.set_delete_from_type(HtmlName::kP);
  ValidateExpected("delete_saving_children_inner",
                   "<div><p>1</p></div><span>2</span>",
                   "<p>1</p><span>2</span>");
  EXPECT_EQ(delete_nodes_filter_.num_start_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_end_elements(), 2);
  EXPECT_EQ(delete_nodes_filter_.num_char_elements(), 2);
  EXPECT_EQ(delete_nodes_filter_.num_deleted_elements(), 1);
}

TEST_F(EventListOrderTest, DeleteSavingChildrenCalledOuter) {
  delete_nodes_filter_.set_delete_on_open_tag(true);
  delete_nodes_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_nodes_filter_.set_delete_from_type(HtmlName::kSpan);
  ValidateExpected("delete_saving_children_outer",
                   "<div><p>1</p></div><span>2</span>",
                   "<p>1</p><span>2</span>");
  EXPECT_EQ(delete_nodes_filter_.num_start_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_end_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_char_elements(), 2);
  EXPECT_EQ(delete_nodes_filter_.num_deleted_elements(), 1);
}

TEST_F(EventListOrderTest, DeleteSavingChildrenCalledInnerMiddle) {
  delete_nodes_filter_.set_delete_on_open_tag(false);
  delete_nodes_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_nodes_filter_.set_delete_from_type(HtmlName::kP);
  ValidateExpected("delete_saving_children_inner_middle",
                   "<div><p>1</p>2<span>3</span></div><span>4</span>",
                   "<p>1</p>2<span>3</span><span>4</span>");
  EXPECT_EQ(delete_nodes_filter_.num_start_elements(), 4);
  EXPECT_EQ(delete_nodes_filter_.num_end_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_char_elements(), 4);
  EXPECT_EQ(delete_nodes_filter_.num_deleted_elements(), 1);
}

TEST_F(EventListOrderTest, DeleteSavingChildrenCalledInnerEnd) {
  delete_nodes_filter_.set_delete_on_open_tag(false);
  delete_nodes_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_nodes_filter_.set_delete_from_type(HtmlName::kP);
  ValidateExpected("delete_saving_children_inner_end",
                   "<div><p>1</p></div><span>2</span>",
                   "<p>1</p><span>2</span>");
  EXPECT_EQ(delete_nodes_filter_.num_start_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_end_elements(), 2);
  EXPECT_EQ(delete_nodes_filter_.num_char_elements(), 2);
  EXPECT_EQ(delete_nodes_filter_.num_deleted_elements(), 1);
}

TEST_F(EventListOrderTest, DeleteSavingChildrenCalledInnerDeep) {
  delete_nodes_filter_.set_delete_on_open_tag(false);
  delete_nodes_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_nodes_filter_.set_delete_from_type(HtmlName::kP);
  ValidateExpected("delete_saving_children_inner_deep",
                   "<div><a><p>1</p>2<span>3</span></a></div><span>4</span>",
                   "<a><p>1</p>2<span>3</span></a><span>4</span>");
  EXPECT_EQ(delete_nodes_filter_.num_start_elements(), 5);
  EXPECT_EQ(delete_nodes_filter_.num_end_elements(), 4);
  EXPECT_EQ(delete_nodes_filter_.num_char_elements(), 4);
  EXPECT_EQ(delete_nodes_filter_.num_deleted_elements(), 1);
}

TEST_F(EventListOrderTest, DeleteSavingChildrenCalledOuterDistant) {
  delete_nodes_filter_.set_delete_on_open_tag(false);
  delete_nodes_filter_.set_delete_node_type(HtmlName::kDiv);
  delete_nodes_filter_.set_delete_from_type(HtmlName::kA);
  ValidateExpected("delete_saving_children_outer_distant",
                   "<div><p>1</p></div><span>2</span><a>3</a>",
                   "<p>1</p><span>2</span><a>3</a>");
  EXPECT_EQ(delete_nodes_filter_.num_start_elements(), 4);
  EXPECT_EQ(delete_nodes_filter_.num_end_elements(), 4);
  EXPECT_EQ(delete_nodes_filter_.num_char_elements(), 3);
  EXPECT_EQ(delete_nodes_filter_.num_deleted_elements(), 1);
}

// Filter to remove nodes during parsing and restore them sometime later.
class RestoreNodesFilter : public CountingCallbacksFilter {
 public:
  explicit RestoreNodesFilter(HtmlParse* html_parse)
      : html_parse_(html_parse),
        outstanding_deferred_elements_(0),
        num_deletes_(0),
        restore_on_open_(false) {
  }

  // Establishes the ID or text of an element to defer, and the ID of an
  // element to move after.
  void MoveOnStart(const char* id_or_text, const char* restore_point) {
    remove_map_[id_or_text] = restore_point;
  }

  void DeleteOnStart(const char* id_or_text) {
    delete_set_.insert(id_or_text);
  }

  void set_restore_on_open(bool restore) {
    restore_on_open_ = restore;
  }

  // Returns the number of nodes that have been deferred, but not yet restored.
  bool AllRestored() const { return restore_map_.empty(); }
  int outstanding_deferred_elements() const {
    return outstanding_deferred_elements_;
  }
  int num_deletes() const { return num_deletes_; }

 protected:
  virtual void StartDocument() {
    CountingCallbacksFilter::StartDocument();
    restore_map_.clear();
    outstanding_deferred_elements_ = 0;
    num_deletes_ = 0;
  }

  virtual void Characters(HtmlCharactersNode* node) {
    CountingCallbacksFilter::Characters(node);
    const GoogleString& text = node->contents();
    if (!MaybeRemoveNode(text, node) &&
        !MaybeDeleteNode(text, node)) {
      MaybeRestoreNode(text);
    }
  }

  virtual void StartElement(HtmlElement* element) {
    CountingCallbacksFilter::StartElement(element);
    const char* id = FindId(element);
    if (id != NULL) {
      if (!MaybeRemoveNode(id, element)) {
        MaybeDeleteNode(id, element);
      }
      if (restore_on_open_) {
        MaybeRestoreNode(id);
      }
    }
  }

  virtual void EndElement(HtmlElement* element) {
    CountingCallbacksFilter::EndElement(element);
    const char* id = FindId(element);
    if (id != NULL && !restore_on_open_) {
      MaybeRestoreNode(id);
    }
  }
  virtual const char* Name() const { return "RestoreNodesFilter"; }

 private:
  typedef std::map<GoogleString, HtmlNode*> RestoreMap;

  const char* FindId(HtmlElement* element) {
    const HtmlElement::Attribute* attr = element->FindAttribute("id");
    if (attr == NULL) {
      return NULL;
    }
    return attr->DecodedValueOrNull();
  }

  bool MaybeRemoveNode(const GoogleString& id, HtmlNode* node) {
    StringStringMap::iterator p = remove_map_.find(id);
    if (p != remove_map_.end()) {
      const GoogleString& restore_id = p->second;
      EXPECT_TRUE(restore_map_[restore_id] == NULL);
      restore_map_[restore_id] = node;
      html_parse_->DeferCurrentNode();
      if (dynamic_cast<HtmlElement*>(node) != NULL) {
        ++outstanding_deferred_elements_;
      }
      return true;
    }
    return false;
  }

  bool MaybeDeleteNode(const GoogleString& id, HtmlNode* node) {
    if (delete_set_.find(id) != delete_set_.end() &&
        html_parse_->DeleteNode(node)) {
      ++num_deletes_;
      return true;
    }
    return false;
  }

  void MaybeRestoreNode(const GoogleString& id) {
    RestoreMap::iterator p = restore_map_.find(id);
    if (p != restore_map_.end()) {
      HtmlNode* restore_node = p->second;
      html_parse_->RestoreDeferredNode(restore_node);
      restore_map_.erase(p);
      if (dynamic_cast<HtmlElement*>(restore_node) != NULL) {
        --outstanding_deferred_elements_;
      }
    }
  }

  HtmlParse* html_parse_;
  StringStringMap remove_map_;
  StringSet delete_set_;
  RestoreMap restore_map_;
  int outstanding_deferred_elements_;
  int num_deletes_;
  bool restore_on_open_;

  DISALLOW_COPY_AND_ASSIGN(RestoreNodesFilter);
};

class HtmlRestoreTest : public HtmlParseTest {
 protected:
  HtmlRestoreTest()
      : upstream_writer_filter_(&html_parse_),
        upstream_writer_(&upstream_buffer_),
        restore_nodes_filter_(&html_parse_),
        expect_restored_(true) {
    // We are interested in the effect on deferring nodes on (a) the
    // filter that does the deferring, (b) upstream filters and (c)
    // downstream filters.  Downstream is covered by the normal
    // HtmlWriterFilter filter that gets installed by SetupWriter.
    // But Upstream is interesting too, especially when a FLUSH occurs
    // while a deferred node is open.  Sowe simply install another
    // writer filter before the RestoreNodesFilter, which should see
    // the input unmodified.
    html_parse_.AddFilter(&upstream_writer_filter_);
    html_parse_.AddFilter(&pre_counts_filter_);
    upstream_writer_filter_.set_writer(&upstream_writer_);
    html_parse_.AddFilter(&restore_nodes_filter_);
    html_parse_.AddFilter(&post_counts_filter_);
  }

  virtual bool AddBody() const { return false; }
  virtual bool AddHtmlTags() const { return false; }

  // Runs a test like ValidateExpected, but puts one or two Flush
  // calls at aribtrary points in the text, covering all n^2 places
  // to put the two flushes.
  //
  // Don't call this with an especially large 'before', otherwise the
  // time taken will grow quadratically.  Calling this with 70 byte
  // inputs appears to be OK, taking <300ms to run even in a debug build.
  void RunTestsWithManyFlushWindows(StringPiece before,
                                    StringPiece expected) {
    SetupWriter();
    int before_size = before.size();
    for (int flush1 = 0; flush1 < before_size; ++flush1) {
      for (int flush2 = flush1; flush2 < before_size; ++flush2) {
        GoogleString this_id =
            StringPrintf("http://test.com/%d_%d", flush1, flush2);
        html_parse_.StartParse(this_id);
        if (flush1 != 0) {
          html_parse_.ParseText(before.substr(0, flush1));
        }
        if (flush2 != flush1) {
          html_parse_.Flush();
          html_parse_.ParseText(before.substr(flush1, flush2 - flush1));
        }
        if (flush2 != before_size) {
          html_parse_.Flush();
          html_parse_.ParseText(before.substr(flush2));
        }
        html_parse_.FinishParse();
        ASSERT_STREQ(expected, output_buffer_) << this_id;
        output_buffer_.clear();
        ASSERT_STREQ(before, upstream_buffer_) << this_id;
        upstream_buffer_.clear();

        // If we expect that everything that was removed was restored, then the
        // start/end/char-counts should all match before and during
        // the RestoreNodes filter.
        if (expect_restored_) {
          EXPECT_TRUE(restore_nodes_filter_.AllRestored()) << this_id;
          if (restore_nodes_filter_.num_deletes() == 0) {
            ASSERT_EQ(pre_counts_filter_.num_start_elements(),
                      restore_nodes_filter_.num_start_elements()) << this_id;
            ASSERT_EQ(pre_counts_filter_.num_end_elements(),
                      restore_nodes_filter_.num_end_elements()) << this_id;
            ASSERT_EQ(pre_counts_filter_.num_start_elements(),
                      post_counts_filter_.num_start_elements()) << this_id;
            ASSERT_EQ(pre_counts_filter_.num_end_elements(),
                      post_counts_filter_.num_end_elements()) << this_id;
          }
          ASSERT_EQ(pre_counts_filter_.num_char_elements(),
                    restore_nodes_filter_.num_char_elements()) << this_id;

          // We use ASSERT_GE here because some of the tests will result in
          // characters being coalesced on the defer or on the restore.
          ASSERT_GE(pre_counts_filter_.num_char_elements(),
                    post_counts_filter_.num_char_elements()) << this_id;

          // Of course, start and end element count must be balanced,
          // as long as all deferred nodes were restored.
          ASSERT_EQ(restore_nodes_filter_.num_start_elements(),
                    (restore_nodes_filter_.num_end_elements() +
                     restore_nodes_filter_.num_deletes())) << this_id;
        } else {
          // Otherwise there will be an extra Start tag for every
          // unrestored element.
          EXPECT_FALSE(restore_nodes_filter_.AllRestored()) << this_id;
          ASSERT_EQ(
              restore_nodes_filter_.num_start_elements(),
              (restore_nodes_filter_.num_end_elements() +
               restore_nodes_filter_.outstanding_deferred_elements()))
              << this_id;
        }

        // Note that only the restore_nodes_filter itself can have mismatched
        // start/end callback-counts.  Filters running before or after that one
        // see a balanced set of callbacks.
        ASSERT_EQ(pre_counts_filter_.num_start_elements(),
                  pre_counts_filter_.num_end_elements()) << this_id;
        ASSERT_EQ(post_counts_filter_.num_start_elements(),
                  post_counts_filter_.num_end_elements()) << this_id;
      }
    }
  }

  void TestTwoFilters(const char* src1, const char* dest1,
                      const char* src2, const char* dest2,
                      const char* node_to_delete,
                      StringPiece input,
                      StringPiece expected) {
    RestoreNodesFilter restore_nodes_filter2(&html_parse_);
    html_parse_.AddFilter(&restore_nodes_filter2);
    SetupWriter();
    restore_nodes_filter_.MoveOnStart(src1, dest1);
    if (node_to_delete != NULL) {
      restore_nodes_filter_.DeleteOnStart(node_to_delete);
    }
    restore_nodes_filter2.MoveOnStart(src2, dest2);
    RunTestsWithManyFlushWindows(input, expected);
  }

  HtmlWriterFilter upstream_writer_filter_;
  CountingCallbacksFilter pre_counts_filter_;
  StringWriter upstream_writer_;
  GoogleString upstream_buffer_;
  RestoreNodesFilter restore_nodes_filter_;
  CountingCallbacksFilter post_counts_filter_;
  bool expect_restored_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HtmlRestoreTest);
};

TEST_F(HtmlRestoreTest, MoveAAfterB) {
  restore_nodes_filter_.MoveOnStart("a", "b");  // moves div 'a' after div 'b'
  RunTestsWithManyFlushWindows(
      ("0<div id=a>1<span>2</span>3</div>"
       "4<div id=b>5<span>6</span></div>7"),
      ("04<div id=b>5<span>6</span></div>"
       "<div id=a>1<span>2</span>3</div>7"));
}

TEST_F(HtmlRestoreTest, MoveAAfterBUnclosed) {
  restore_nodes_filter_.MoveOnStart("a", "b");  // moves div 'a' after div 'b'
  RunTestsWithManyFlushWindows(
      ("0<div id=a>1<span>2</span>3</div>"
       "4<div id=b>5<span>6</span>7"),  // b unclosed, but lexer auto-closes it.
      ("04<div id=b>5<span>6</span>7"
       "<div id=a>1<span>2</span>3</div>"));
}

TEST_F(HtmlRestoreTest, MoveAAfterNestedB) {
  restore_nodes_filter_.MoveOnStart("a", "b");  // moves div 'a' after div 'b'
  RunTestsWithManyFlushWindows(
      ("0<div id=a>1<span>2</span>3</div>"
       "4<div><div id=b>5<span>6</span></div>7</div>"),
      ("04<div><div id=b>5<span>6</span></div>"
       "<div id=a>1<span>2</span>3</div>7</div>"));
}


TEST_F(HtmlRestoreTest, MoveABAfterC) {
  restore_nodes_filter_.MoveOnStart("a", "c");
  restore_nodes_filter_.MoveOnStart("b", "a");
  RunTestsWithManyFlushWindows(
      "0<img id=a />1<img id=b />2<img id=c />3",
      "012<img id=c /><img id=a /><img id=b />3");
}

TEST_F(HtmlRestoreTest, MoveTextAfterDiv) {
  restore_nodes_filter_.MoveOnStart("start", "a");
  RunTestsWithManyFlushWindows(
      "start<div id=a></div>",
      "<div id=a></div>start");
}

TEST_F(HtmlRestoreTest, MoveDivAfterText) {
  restore_nodes_filter_.MoveOnStart("a", "hello");
  RunTestsWithManyFlushWindows(
      "<div id=a></div>hello",
      "hello<div id=a></div>");
}

TEST_F(HtmlRestoreTest, MoveTextfterText) {
  restore_nodes_filter_.MoveOnStart("one", "two");
  RunTestsWithManyFlushWindows("one<p>two", "<p>twoone");
}

TEST_F(HtmlRestoreTest, MoveStartWithEndNotVisibleAUnclosed) {
  message_handler_.AddPatternToSkipPrinting(
      "*Removed node <div id=a> (unclosed)*");
  SetupWriter();
  restore_nodes_filter_.MoveOnStart("a", "b");
  expect_restored_ = false;
  RunTestsWithManyFlushWindows("<div id=a>1<div id=b>2</div>", "");
  EXPECT_LT(0, message_handler_.MessagesOfType(kWarning));
}

TEST_F(HtmlRestoreTest, MoveDivWithMissingDestination) {
  message_handler_.AddPatternToSkipPrinting("*Removed node <div id=a></div>*");
  SetupWriter();
  restore_nodes_filter_.MoveOnStart("a", "b");
  expect_restored_ = false;
  RunTestsWithManyFlushWindows("<div id=a>1</div>", "");
  EXPECT_LT(0, message_handler_.MessagesOfType(kWarning));
}

TEST_F(HtmlRestoreTest, MoveCharsWithMissingDestination) {
  message_handler_.AddPatternToSkipPrinting(
      "*Removed node Characters text never replaced*");
  SetupWriter();
  restore_nodes_filter_.MoveOnStart("text", "no_such_destination");
  expect_restored_ = false;
  RunTestsWithManyFlushWindows("text", "");
  EXPECT_LT(0, message_handler_.MessagesOfType(kWarning));
}

TEST_F(HtmlRestoreTest, TwoDeleteAcrossFlush) {
  SetupWriter();
  restore_nodes_filter_.DeleteOnStart("a");
  RunTestsWithManyFlushWindows("1<div id=a></div>2", "12");
}

TEST_F(HtmlRestoreTest, RestoreOnOpenTag) {
  SetupWriter();
  restore_nodes_filter_.MoveOnStart("a", "b");
  restore_nodes_filter_.set_restore_on_open(true);
  RunTestsWithManyFlushWindows("<div id=a>abc</div><div id=b>def</div>",
                               "<div id=b><div id=a>abc</div>def</div>");
}

// This tests having two filters that each do deferrals.  The
// interesting case is where the second filter in the chain defers a
// node first, and then, before restoring first deferred node, another
// filter defers a different node.
TEST_F(HtmlRestoreTest, TwoDeferringFilters) {
  TestTwoFilters(
      "b", "c",
      "a", "d",
      NULL,  // Node to delete
      "<img id=a /><img id=b /><img id=c /><img id=d />",
      "<img id=c /><img id=b /><img id=d /><img id=a />");
}

TEST_F(HtmlRestoreTest, TwoDeferringFiltersWithDelete) {
  TestTwoFilters(
      "b", "c",  // In first filter, mov div b to after to div c.
      "a", "d",  // In second filter, move div a to after div d.
      "a",       // In first filter, delete node "a"
      "<img id=a /><img id=b /><img id=c /><img id=d />",
      "<img id=c /><img id=b /><img id=d />");
}

TEST_F(HtmlRestoreTest, TwoDeferringFiltersNestingOuterFirst) {
  TestTwoFilters(
      "a", "d",
      "b", "c",
      NULL,  // Node to delete
      "<div id=a><div id=b></div><div id=c></div></div><div id=d></div>",
      "<div id=d></div><div id=a><div id=c></div><div id=b></div></div>");
}

TEST_F(HtmlRestoreTest, TwoDeferringFiltersNestingOuterFirstWithDelete) {
  TestTwoFilters(
      "a", "d",
      "b", "c",
      "b",
      "<div id=a><div id=b></div><div id=c></div></div><div id=d></div>",
      "<div id=d></div><div id=a><div id=c></div></div>");
}

TEST_F(HtmlRestoreTest, TwoDeferringFiltersNestingInnerFirst) {
  TestTwoFilters(
      "b", "c",
      "a", "d",
      NULL,  // Node to delete
      "<div id=a><div id=b></div><div id=c></div></div><div id=d></div>",
      "<div id=d></div><div id=a><div id=c></div><div id=b></div></div>");
}

TEST_F(HtmlRestoreTest, TwoDeferringFiltersNestingInnerFirstWithDelete) {
  TestTwoFilters(
      "b", "c",
      "a", "d",
      "a",
      "<div id=a><div id=b></div><div id=c></div></div><div id=d></div>",
      "<div id=d></div>");
}

TEST_F(HtmlRestoreTest, DeferringAndDeletingFilters) {
  DeleteNodesFilter delete_nodes_filter(&html_parse_);
  RestoreNodesFilter restore_nodes_filter2(&html_parse_);
  html_parse_.AddFilter(&delete_nodes_filter);  // Upstream
  html_parse_.AddFilter(&restore_nodes_filter2);  // Downstream
  SetupWriter();
  // Don't do anything with restore_nodes_filter_
  delete_nodes_filter.set_delete_node_type(HtmlName::kSpan);
  delete_nodes_filter.set_delete_from_type(HtmlName::kDiv);
  delete_nodes_filter.set_delete_on_open_tag(true);
  restore_nodes_filter2.MoveOnStart("a", "d");
  const StringPiece kInput(
      "<span id=a><div id=b></div><div id=c></div></span><div id=d></div>");
  ValidateExpected("defer_and_delete", kInput,
                   "<div id=b></div><div id=c></div><div id=d></div>");
  EXPECT_EQ(1, delete_nodes_filter.num_deleted_elements());

  // With the same filter setup, put a flush in the middle.
  output_buffer_.clear();
  html_parse_.StartParse("http://test.com/with_flush");
  html_parse_.ParseText(kInput.substr(0, kInput.size() / 2));
  html_parse_.Flush();
  html_parse_.ParseText(kInput.substr(kInput.size() / 2));
  html_parse_.FinishParse();

  // Because of the flush, deleting the 'span' did not work.  However,
  // moving the span (and all its contents) after the 'd' did.
  //
  // TODO(jmarantz): consider making DeleteSavingChildren work even if
  // the EndElement is not yet parsed, in which case we can switch to
  // using RunTestsWithManyFlushWindows and expect the same results
  // regardless of when the flush occurs.
  EXPECT_STREQ(
      "<div id=d></div><span id=a><div id=b></div><div id=c></div></span>",
      output_buffer_);
  EXPECT_EQ(0, delete_nodes_filter.num_deleted_elements());
}

TEST_F(HtmlRestoreTest, DeleteDeferredNode) {
  DeleteNodesFilter delete_nodes_filter(&html_parse_);
  RestoreNodesFilter restore_nodes_filter2(&html_parse_);
  html_parse_.AddFilter(&delete_nodes_filter);  // Upstream
  html_parse_.AddFilter(&restore_nodes_filter2);  // Downstream
  SetupWriter();
  // Don't do anything with restore_nodes_filter_
  delete_nodes_filter.set_delete_node_type(HtmlName::kSpan);
  delete_nodes_filter.set_delete_from_type(HtmlName::kDiv);
  delete_nodes_filter.set_delete_on_open_tag(true);
  restore_nodes_filter2.MoveOnStart("a", "d");
  const StringPiece kInput("<span id=a></span><div id=d></div>");
  ValidateExpected("delete_deferred", kInput, "<div id=d></div>");
  EXPECT_EQ(1, delete_nodes_filter.num_deleted_elements());
  EXPECT_EQ(0, delete_nodes_filter.flushes_preventing_delete());

  // With the same filter setup, put a flush in the middle.
  output_buffer_.clear();
  html_parse_.StartParse("http://test.com/with_flush");
  html_parse_.ParseText(kInput.substr(0, kInput.size() / 2));
  html_parse_.Flush();
  html_parse_.ParseText(kInput.substr(kInput.size() / 2));
  html_parse_.FinishParse();

  // Because of the flush, deleting the 'span' did not work.  However,
  // moving the span after the 'd' did.
  //
  // TODO(jmarantz): consider making DeleteSavingChildren work even if
  // the EndElement is not yet parsed, in which case we can switch to
  // using RunTestsWithManyFlushWindows and expect the same results
  // regardless of when the flush occurs.
  EXPECT_STREQ("<div id=d></div><span id=a></span>", output_buffer_);
  EXPECT_EQ(0, delete_nodes_filter.num_deleted_elements());
  EXPECT_EQ(1, delete_nodes_filter.flushes_preventing_delete());
}

TEST_F(HtmlRestoreTest, CoalesceCharsAfterRestore) {
  restore_nodes_filter_.MoveOnStart("1", "a");
  SetupWriter();

  const StringPiece kInput("1<img id=a />2");
  int num_times_chars_are_coalesced = 0;
  int num_times_chars_are_not_coalesced = 0;
  for (int i = 0, n = kInput.size(); i < n; ++i) {
    ParseWithFlush(kInput, i);
    EXPECT_STREQ("<img id=a />12", output_buffer_) << i;

    // Before the deferral, we had two Characters nodes.
    EXPECT_EQ(2, pre_counts_filter_.num_char_elements()) << i;

    // The filter that does the deferring also sees two Characters nodes.
    EXPECT_EQ(2, restore_nodes_filter_.num_char_elements()) << i;

    // After the restore, the Characters nodes may be coalesced,
    // depending on the flush window.
    EXPECT_TRUE((post_counts_filter_.num_char_elements() == 1) ||
                (post_counts_filter_.num_char_elements() == 2)) << i;
    if (post_counts_filter_.num_char_elements() == 1) {
      ++num_times_chars_are_coalesced;
    } else {
      ++num_times_chars_are_not_coalesced;
    }
  }
  EXPECT_LT(0, num_times_chars_are_coalesced);
  EXPECT_LT(0, num_times_chars_are_not_coalesced);
}

TEST_F(HtmlRestoreTest, CoalesceCharsOnDefer) {
  restore_nodes_filter_.MoveOnStart("a", "b");
  SetupWriter();

  const StringPiece kInput("1<img id=a />2<p id=b />");
  int num_times_chars_are_coalesced = 0;
  int num_times_chars_are_not_coalesced = 0;
  for (int i = 0, n = kInput.size(); i < n; ++i) {
    ParseWithFlush(kInput, i);
    EXPECT_STREQ(output_buffer_, "12<p id=b /><img id=a />");

    // Before the deferral, we had two Characters nodes.
    EXPECT_EQ(2, pre_counts_filter_.num_char_elements()) << i;

    // The filter that does the deferring also sees two Characters nodes.
    EXPECT_EQ(2, restore_nodes_filter_.num_char_elements()) << i;

    // After the restore, the Characters nodes may be coalesced,
    // depending on the flush window.

    // After the deferral, the Characters nodes may be coalesced,
    // depending on the flush window.
    EXPECT_TRUE((post_counts_filter_.num_char_elements() == 1) ||
                (post_counts_filter_.num_char_elements() == 2)) << i;
    if (post_counts_filter_.num_char_elements() == 1) {
      ++num_times_chars_are_coalesced;
    } else {
      ++num_times_chars_are_not_coalesced;
    }
  }
  EXPECT_LT(0, num_times_chars_are_coalesced);
  EXPECT_LT(0, num_times_chars_are_not_coalesced);
}

// This test just shows that the lexer will, in the absense of Defer or
// Delete calls, coalesce Characters nodes across Flush.  It does this
// by being lazy and not emitting literals until it seems some HTML syntax.
TEST_F(HtmlRestoreTest, CoalesceCharsAcrossFlush) {
  SetupWriter();

  const StringPiece kInput("12");
  for (int i = 0, n = kInput.size(); i < n; ++i) {
    ParseWithFlush(kInput, i);
    EXPECT_STREQ("12", output_buffer_) << i;
    EXPECT_EQ(1, pre_counts_filter_.num_char_elements()) << i;
  }
}

class InsertScriptsFilter : public EmptyHtmlFilter {
 public:
  explicit InsertScriptsFilter(HtmlParse* parse)
      : html_parse_(parse),
        at_start_(false),
        before_(false),
        external_(false) {
  }

  void set_insert_before(bool before) { before_ = before; }
  void set_at_start(bool at_start) { at_start_ = at_start; }
  void set_external(bool external) { external_ = external; }

 protected:
  virtual void StartElement(HtmlElement* element) { Insert(true, element); }
  virtual void EndElement(HtmlElement* element) { Insert(false, element); }
  virtual const char* Name() const { return "InsertScriptsFilter"; }
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  void Insert(bool at_start, HtmlElement* element) {
    if (element->keyword() == HtmlName::kHead) {
      if (at_start == at_start_) {
        if (before_) {
          html_parse_->InsertScriptBeforeCurrent("inserted", external_);
        } else {
          html_parse_->InsertScriptAfterCurrent("inserted", external_);
        }
      }
    }
  }

 private:
  HtmlParse* html_parse_;
  bool at_start_;
  bool before_;
  bool external_;

  DISALLOW_COPY_AND_ASSIGN(InsertScriptsFilter);
};

TEST_F(HtmlParseTestNoBody, InsertInlineScriptAfterStartOfHead) {
  InsertScriptsFilter insert_scripts(&html_parse_);
  insert_scripts.set_insert_before(false);
  insert_scripts.set_at_start(true);
  insert_scripts.set_external(false);
  html_parse_.AddFilter(&insert_scripts);
  SetupWriter();
  ValidateExpected("1",
                   "<head>text</head>",
                   "<head><script>inserted</script>text</head>");
}

TEST_F(HtmlParseTestNoBody, InsertInlineScriptBeforeEndOfHead) {
  InsertScriptsFilter insert_scripts(&html_parse_);
  insert_scripts.set_insert_before(true);
  insert_scripts.set_at_start(false);
  insert_scripts.set_external(false);
  html_parse_.AddFilter(&insert_scripts);
  SetupWriter();
  ValidateExpected("1",
                   "<head>text</head>",
                   "<head>text<script>inserted</script></head>");
}

TEST_F(HtmlParseTestNoBody, InsertInlineScriptBeforeStartOfHead) {
  InsertScriptsFilter insert_scripts(&html_parse_);
  insert_scripts.set_insert_before(true);
  insert_scripts.set_at_start(true);
  insert_scripts.set_external(false);
  html_parse_.AddFilter(&insert_scripts);
  SetupWriter();
  ValidateExpected("1",
                   "<head>text</head>",
                   "<script>inserted</script><head>text</head>");
}

TEST_F(HtmlParseTestNoBody, InsertInlineScriptAfterEndOfHead) {
  InsertScriptsFilter insert_scripts(&html_parse_);
  insert_scripts.set_insert_before(false);
  insert_scripts.set_at_start(false);
  insert_scripts.set_external(false);
  html_parse_.AddFilter(&insert_scripts);
  SetupWriter();
  ValidateExpected("1",
                   "<head>text</head>",
                   "<head>text</head><script>inserted</script>");
}

TEST_F(HtmlParseTestNoBody, InsertExternalScriptAfterStartOfHead) {
  InsertScriptsFilter insert_scripts(&html_parse_);
  insert_scripts.set_insert_before(false);
  insert_scripts.set_at_start(true);
  insert_scripts.set_external(true);
  html_parse_.AddFilter(&insert_scripts);
  SetupWriter();
  ValidateExpected("1",
                   "<head>text</head>",
                   "<head><script src=\"inserted\"></script>text</head>");
}

TEST_F(HtmlParseTestNoBody, InsertExternalScriptBeforeEndOfHead) {
  InsertScriptsFilter insert_scripts(&html_parse_);
  insert_scripts.set_insert_before(true);
  insert_scripts.set_at_start(false);
  insert_scripts.set_external(true);
  html_parse_.AddFilter(&insert_scripts);
  SetupWriter();
  ValidateExpected("1",
                   "<head>text</head>",
                   "<head>text<script src=\"inserted\"></script></head>");
}

TEST_F(HtmlParseTestNoBody, InsertExternalScriptBeforeStartOfHead) {
  InsertScriptsFilter insert_scripts(&html_parse_);
  insert_scripts.set_insert_before(true);
  insert_scripts.set_at_start(true);
  insert_scripts.set_external(true);
  html_parse_.AddFilter(&insert_scripts);
  SetupWriter();
  ValidateExpected("1",
                   "<head>text</head>",
                   "<script src=\"inserted\"></script><head>text</head>");
}

TEST_F(HtmlParseTestNoBody, InsertExternalScriptAfterEndOfHead) {
  InsertScriptsFilter insert_scripts(&html_parse_);
  insert_scripts.set_insert_before(false);
  insert_scripts.set_at_start(false);
  insert_scripts.set_external(true);
  html_parse_.AddFilter(&insert_scripts);
  SetupWriter();
  ValidateExpected("1",
                   "<head>text</head>",
                   "<head>text</head><script src=\"inserted\"></script>");
}


}  // namespace net_instaweb
