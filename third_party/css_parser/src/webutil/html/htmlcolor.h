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
//
// .h for the HtmlColor class
// HtmlColor provides 'IsSimilar' for comparing HTML colors
//   of different representations ( "#xxxxxx" or color names such as "white").
// Before doing comparison, check 'IsDefined' first, it's because
//   that not all given HTML color strings are valid.

#ifndef WEBUTIL_HTML_HTMLCOLOR_H_
#define WEBUTIL_HTML_HTMLCOLOR_H_

#include <stdlib.h>
#include <string>
#include "string_using.h"
#include "strings/stringpiece.h"

class HtmlColor {
 private:
  unsigned char r_;
  unsigned char g_;
  unsigned char b_;

  //  a color is well defined if it is "#xxxxxx" ('x' is a hexdigit)
  //  or it's a known color name such as "black"
  //  or it was set from valid HSL values.
  //  0: the RGB value is good!
  //  1: bad (name) value, caused by bad color name
  //  2: bad (hex) value, caused by bad hex string value
  //  -- the browser (Netscape Communicator 4.75, linux-2.2.14,
  //     shows that the color displayed is sometimes 'black' under case '2'.
  //  3: bad HSL values
  unsigned char is_bad_value_;
  static const unsigned char kGoodColorValue = 0;
  static const unsigned char kBadColorName = 1;
  static const unsigned char kBadColorHex = 2;
  static const unsigned char kBadColorHSL = 3;

  void SetBadNameValue() {
    r_ = g_ = b_ = 0;
    is_bad_value_ = kBadColorName;
  }
  void SetBadHexValue() {
    r_ = g_ = b_ = 0;
    is_bad_value_ = kBadColorHex;
  }
  void SetBadHslValue() {
    r_ = g_ = b_ = 0;
    is_bad_value_ = kBadColorHSL;
  }

 public:
  enum TolerateLevel { EXACTLY_SAME = 0, HIGHLY_SIMILAR = 5, SIMILAR = 10 };

  // These methods also accept a CSS shorthand string "#xyz" for convenience.
  // "#xyz" is expanded to "#xxyyzz" before processing.
  explicit HtmlColor(StringPiece colorstr);
  HtmlColor(const char* colorstr, int colorstrlen);
  HtmlColor(unsigned char r, unsigned char g, unsigned char b);

  bool IsDefined() const { return is_bad_value_ == 0; }

  bool IsSimilar(const HtmlColor& color, int level) const {
    if (!IsDefined() || !color.IsDefined()) return false;

    if ((abs(static_cast<int>(r_) - static_cast<int>(color.r_)) <= level) &&
        (abs(static_cast<int>(g_) - static_cast<int>(color.g_)) <= level) &&
        (abs(static_cast<int>(b_) - static_cast<int>(color.b_)) <= level))
      return true;
    return false;
  }

  // Compares color similarity in HSL (Hue, Saturation, Lightness) space.
  // This is assumed to be more accurate based on human perception.
  // Note the difference level is a float number and it may vary from 0.0 to
  // 1.0, inclusive. A suggested value for level is 0.02.
  // WARNING: this is more expensive than IsSimilar() as it involves float
  // arithmetic and cosine operations.
  // TODO(yian): may need to disintegrate it into a separate HSL class.
  bool IsSimilarInHSL(const HtmlColor& color, double level) const;

  // return the luminance (0-255) of the color.
  // this corresponds to a human's perception of the color's brightness
  int Luminance() const;

  // Lighten or darken the color by a given factor (between 0 and 1)
  // Lightening with factor 1.0 => white
  // Darkening with factor 1.0 => black
  void Lighten(float factor);
  void Darken(float factor);

  // Desaturate the color (0.0 = no change, 1.0 = equivalent shade of gray)
  void Desaturate(float factor);

  // Blend the color with a second color by a certain factor between 0 and 1
  // 1.0 => original color
  // 0.0 => other color
  void BlendWithColor(float factor, const HtmlColor& c);

  string ToString() const;

  // hexstr is in form of "xxxxxx"
  void SetValueFromHexStr(StringPiece hexstr);

  // either a color name or a hex string "#xxxxxx"
  // This method also accepts a CSS shorthand string "#xyz" for convenience.
  // "#xyz" is expanded to "#xxyyzz" before processing.
  void SetValueFromStr(StringPiece str);

  // Set the html color object from rgb values
  void SetValueFromRGB(unsigned char r, unsigned char g, unsigned char b);

  // Set the html color object from hsl values, hue must be in [0, 1),
  // saturation and lightness in [0, 1] otherwise IsDefined will return false.;
  // WARNING: This is more expensive than SetValueFromRGB.
  void SetValueFromHSL(double hue, double saturation, double lightness);

  // Converts a valid number to HSL color space. hue is in [0, 1), saturation
  // and lightness in [0, 1].
  // When IsDefined is false this method does not modify the value of hue,
  // saturation and lightness and returns false.
  bool GetValueInHSL(double* hue, double* saturation, double* lightness) const;

  // must be a color name. It can be one of 147 colors defined in CSS3 color
  // module or SVG 1.0, which is supported by all major browsers. A reference
  // can be found at:
  //   http://www.w3.org/TR/css3-color/#svg-color
  void SetValueFromName(StringPiece str);

  // Two IsDefined() colors are equal if their rgb()s are equal.
  // An IsDefined() color is not equal to a !IsDefined() color.
  // Two !IsDefined() colors are equal regardless of their rgb()s.
  bool Equals(const HtmlColor& color) const;

  int r() const { return static_cast<int>(r_); }
  int g() const { return static_cast<int>(g_); }
  int b() const { return static_cast<int>(b_); }
  int rgb() const { return b() + (g() << 8) + (r() << 16); }
};

class HtmlColorUtils {
 public:
  // Converts a color into its shortest possible CSS representation.
  // For 9 colors, that is their color name. Example: "#008000" returns "green".
  // For colors in the form #rrggbb, where r=r, g=g, and b=b, that is #rgb.
  // Example: "#aabbcc" returns "#abc".
  // For all other colors, the six hex-digit representation is shortest.
  // Example: "lightgoldenrodyellow" returns "#FAFAD2"
  static string MaybeConvertToCssShorthand(const HtmlColor& color);
  static string MaybeConvertToCssShorthand(StringPiece orig);
};

#endif  // WEBUTIL_HTML_HTMLCOLOR_H_
