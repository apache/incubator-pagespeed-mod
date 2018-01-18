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

(function(){window.pagespeed=window.pagespeed||{};var d=window.pagespeed;function g(a){this.a=a}function h(a,b){b=b||window.event;if("keypress"!=b.type||13==b.keyCode)for(var c=b.target;null!=c;c=c.parentNode)if("A"==c.tagName){for(var f=a,c=c.href,k=b,e=0;e<f.a.length;e++)if(!c.indexOf(f.a[e])){window.location=window.location.protocol+"//"+window.location.hostname+"/"+c.substr(f.a[e].length);k.preventDefault();break}break}}
function l(a){document.body.onclick=function(b){h(a,b)};document.body.onkeypress=function(b){h(a,b)}}d.b=function(a){a=new g(a);d.clientDomainRewriter=a;l(a)};d.clientDomainRewriterInit=d.b;})();
