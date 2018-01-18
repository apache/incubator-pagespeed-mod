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

(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DelayImagesInline = function() {
  this.inlineMap_ = {};
};
pagespeed.DelayImagesInline.prototype.addLowResImages = function(a, b) {
  this.inlineMap_[a] = b;
};
pagespeed.DelayImagesInline.prototype.addLowResImages = pagespeed.DelayImagesInline.prototype.addLowResImages;
pagespeed.DelayImagesInline.prototype.replaceElementSrc = function(a) {
  for (var b = 0;b < a.length;++b) {
    var c = a[b].getAttribute("data-pagespeed-high-res-src"), d = a[b].getAttribute("src");
    c && !d && (c = this.inlineMap_[c]) && a[b].setAttribute("src", c);
  }
};
pagespeed.DelayImagesInline.prototype.replaceElementSrc = pagespeed.DelayImagesInline.prototype.replaceElementSrc;
pagespeed.DelayImagesInline.prototype.replaceWithLowRes = function() {
  this.replaceElementSrc(document.getElementsByTagName("img"));
  this.replaceElementSrc(document.getElementsByTagName("input"));
};
pagespeed.DelayImagesInline.prototype.replaceWithLowRes = pagespeed.DelayImagesInline.prototype.replaceWithLowRes;
pagespeed.delayImagesInlineInit = function() {
  var a = new pagespeed.DelayImagesInline;
  pagespeed.delayImagesInline = a;
};
pagespeed.delayImagesInlineInit = pagespeed.delayImagesInlineInit;
})();
