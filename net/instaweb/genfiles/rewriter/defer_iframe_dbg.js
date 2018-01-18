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
pagespeed.DeferIframe = function() {
};
pagespeed.DeferIframe.prototype.convertToIframe = function() {
  var a = document.getElementsByTagName("pagespeed_iframe");
  if (0 < a.length) {
    for (var a = a[0], d = document.createElement("iframe"), b = 0, c = a.attributes, e = c.length;b < e;++b) {
      d.setAttribute(c[b].name, c[b].value);
    }
    a.parentNode.replaceChild(d, a);
  }
};
pagespeed.DeferIframe.prototype.convertToIframe = pagespeed.DeferIframe.prototype.convertToIframe;
pagespeed.deferIframeInit = function() {
  var a = new pagespeed.DeferIframe;
  pagespeed.deferIframe = a;
};
pagespeed.deferIframeInit = pagespeed.deferIframeInit;
})();
