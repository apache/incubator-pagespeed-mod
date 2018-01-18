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

(function(){var ENTER_KEY_CODE = 13;
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.ClientDomainRewriter = function(a) {
  this.mappedDomainNames_ = a;
};
pagespeed.ClientDomainRewriter.prototype.anchorListener = function(a) {
  a = a || window.event;
  if ("keypress" != a.type || a.keyCode == ENTER_KEY_CODE) {
    for (var b = a.target;null != b;b = b.parentNode) {
      if ("A" == b.tagName) {
        this.processEvent(b.href, a);
        break;
      }
    }
  }
};
pagespeed.ClientDomainRewriter.prototype.addEventListeners = function() {
  var a = this;
  document.body.onclick = function(b) {
    a.anchorListener(b);
  };
  document.body.onkeypress = function(b) {
    a.anchorListener(b);
  };
};
pagespeed.ClientDomainRewriter.prototype.processEvent = function(a, b) {
  for (var c = 0;c < this.mappedDomainNames_.length;c++) {
    if (0 == a.indexOf(this.mappedDomainNames_[c])) {
      window.location = window.location.protocol + "//" + window.location.hostname + "/" + a.substr(this.mappedDomainNames_[c].length);
      b.preventDefault();
      break;
    }
  }
};
pagespeed.clientDomainRewriterInit = function(a) {
  a = new pagespeed.ClientDomainRewriter(a);
  pagespeed.clientDomainRewriter = a;
  a.addEventListeners();
};
pagespeed.clientDomainRewriterInit = pagespeed.clientDomainRewriterInit;
})();
