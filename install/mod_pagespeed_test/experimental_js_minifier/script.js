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

/**
 * @fileoverview  Example file to print out several JS-accessible browser notes.
 */
// This script is just an example used for system testing. We don't
// really care if it is accurate or not.
// This comment will be removed.
/* This one will also be removed. */

console.log('External script start');

// Browser/window data to report
var data = { 'User-Agent': navigator.userAgent,
             'Platform': navigator.platform,
             'Date': window.Date,
             'History': window.History,
             'Location': window.Location,
             'Dimensions': window.outerHeight + 'x' + window.outerWidth,
             'Is IE?': is.ie,
             'Is Netscape?': is.ns,
             'Is Opera?': is.opera,
             'Is Gecko?': is.gecko,
             'Is Windows?': is.win,
             'Is Mac?': is.mac
           };

// Construct HTML list for data.
var dl = document.createElement('dl');
for (key in data) {
  var dt = document.createElement('dt');
  dt.innerText = key;

  var dd = document.createElement('dd');
  dd.innerText = data[key];

  dl.appendChild(dt);
  dl.appendChild(dd);
}

// Add list to page.
var content = document.getElementById('content');
content.appendChild(dl);

console.log('External script finish');
