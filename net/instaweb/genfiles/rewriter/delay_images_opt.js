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

(function(){function c(a,b,d){if(a.addEventListener)a.addEventListener(b,d,!1);else if(a.attachEvent)a.attachEvent("on"+b,d);else{var e=a["on"+b];a["on"+b]=function(){d.call(this);e&&e.call(this)}}}var f=Date.now||function(){return+new Date};window.pagespeed=window.pagespeed||{};var g=window.pagespeed;function h(){this.a=this.c=!1}h.prototype.b=function(a){for(var b=0;b<a.length;++b){var d=a[b].getAttribute("data-pagespeed-high-res-src");d&&a[b].setAttribute("src",d)}};h.prototype.replaceElementSrc=h.prototype.b;
h.prototype.h=function(){if(this.c)this.a=!1;else{var a=document.body,b,d=0,e=this;"ontouchstart"in a?(c(a,"touchstart",function(){b=f()}),c(a,"touchend",function(a){d=f();(null!=a.changedTouches&&2==a.changedTouches.length||null!=a.touches&&2==a.touches.length||500>d-b)&&k(e)})):c(window,"click",function(){k(e)});c(window,"load",function(){k(e)});this.c=!0}};h.prototype.registerLazyLoadHighRes=h.prototype.h;function k(a){a.a||(a.f(),a.a=!0)}
h.prototype.f=function(){this.b(document.getElementsByTagName("img"));this.b(document.getElementsByTagName("input"))};h.prototype.replaceWithHighRes=h.prototype.f;g.g=function(){g.delayImages=new h};g.delayImagesInit=g.g;})();
