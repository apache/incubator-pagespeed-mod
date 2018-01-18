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

(function(){window.pagespeed=window.pagespeed||{};var l=window.pagespeed;
l.getResourceTimingData=function(){if(window.performance&&(window.performance.getEntries||window.performance.webkitGetEntries)){for(var n=0,m=0,e=0,p=0,f=0,q=0,g=0,r=0,h=0,t=0,k=0,c={},d=window.performance.getEntries?window.performance.getEntries():window.performance.webkitGetEntries(),b=0;b<d.length;b++){var a=d[b].duration;0<a&&(n+=a,++e,m=Math.max(m,a));a=d[b].connectEnd-d[b].connectStart;0<a&&(q+=a,++g);a=d[b].domainLookupEnd-d[b].domainLookupStart;0<a&&(p+=a,++f);a=d[b].initiatorType;c[a]?++c[a]:
c[a]=1;a=d[b].requestStart-d[b].fetchStart;0<a&&(t+=a,++k);a=d[b].responseStart-d[b].requestStart;0<a&&(r+=a,++h)}return"&afd="+(e?Math.round(n/e):0)+"&nfd="+e+"&mfd="+Math.round(m)+"&act="+(g?Math.round(q/g):0)+"&nct="+g+"&adt="+(f?Math.round(p/f):0)+"&ndt="+f+"&abt="+(k?Math.round(t/k):0)+"&nbt="+k+"&attfb="+(h?Math.round(r/h):0)+"&nttfb="+h+(c.css?"&rit_css="+c.css:"")+(c.link?"&rit_link="+c.link:"")+(c.script?"&rit_script="+c.script:"")+(c.img?"&rit_img="+c.img:"")}return""};
l.getResourceTimingData=l.getResourceTimingData;})();
