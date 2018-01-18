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

(function(){function f(b){var a=window;if(a.addEventListener)a.addEventListener("load",b,!1);else if(a.attachEvent)a.attachEvent("onload",b);else{var c=a.onload;a.onload=function(){b.call(this);c&&c.call(this)}}};window.pagespeed=window.pagespeed||{};var k=window.pagespeed;function l(b,a,c,g,h){this.h=b;this.i=a;this.l=c;this.j=g;this.b=h;this.c=[];this.a=0}l.prototype.f=function(b){for(var a=0;250>a&&this.a<this.b.length;++a,++this.a)try{document.querySelector(this.b[this.a])&&this.c.push(this.b[this.a])}catch(c){}this.a<this.b.length?window.setTimeout(this.f.bind(this),0,b):b()};
k.g=function(b,a,c,g,h){if(document.querySelector&&Function.prototype.bind){var d=new l(b,a,c,g,h);f(function(){window.setTimeout(function(){d.f(function(){for(var a="oh="+d.l+"&n="+d.j,a=a+"&cs=",b=0;b<d.c.length;++b){var c=0<b?",":"",c=c+encodeURIComponent(d.c[b]);if(131072<a.length+c.length)break;a+=c}k.criticalCssBeaconData=a;var b=d.h,c=d.i,e;if(window.XMLHttpRequest)e=new XMLHttpRequest;else if(window.ActiveXObject)try{e=new ActiveXObject("Msxml2.XMLHTTP")}catch(m){try{e=new ActiveXObject("Microsoft.XMLHTTP")}catch(n){}}e&&
(e.open("POST",b+(-1==b.indexOf("?")?"?":"&")+"url="+encodeURIComponent(c)),e.setRequestHeader("Content-Type","application/x-www-form-urlencoded"),e.send(a))})},0)})}};k.criticalCssBeaconInit=k.g;})();
