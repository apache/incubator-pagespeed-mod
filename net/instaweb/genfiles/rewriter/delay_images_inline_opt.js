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

(function(){window.pagespeed=window.pagespeed||{};var a=window.pagespeed;function d(){this.b={}}d.prototype.c=function(c,b){this.b[c]=b};d.prototype.addLowResImages=d.prototype.c;d.prototype.a=function(c){for(var b=0;b<c.length;++b){var e=c[b].getAttribute("data-pagespeed-high-res-src"),f=c[b].getAttribute("src");e&&!f&&(e=this.b[e])&&c[b].setAttribute("src",e)}};d.prototype.replaceElementSrc=d.prototype.a;d.prototype.g=function(){this.a(document.getElementsByTagName("img"));this.a(document.getElementsByTagName("input"))};
d.prototype.replaceWithLowRes=d.prototype.g;a.f=function(){a.delayImagesInline=new d};a.delayImagesInlineInit=a.f;})();
