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
pagespeed.DedupInlinedImages = function() {
};
pagespeed.DedupInlinedImages.prototype.inlineImg = function(a, c, b) {
  if (a = document.getElementById(a)) {
    if (c = document.getElementById(c)) {
      if (b = document.getElementById(b)) {
        c.src = a.getAttribute("src"), b.parentNode.removeChild(b);
      }
    }
  }
};
pagespeed.DedupInlinedImages.prototype.inlineImg = pagespeed.DedupInlinedImages.prototype.inlineImg;
pagespeed.dedupInlinedImagesInit = function() {
  var a = new pagespeed.DedupInlinedImages;
  pagespeed.dedupInlinedImages = a;
};
pagespeed.dedupInlinedImagesInit = pagespeed.dedupInlinedImagesInit;
})();
