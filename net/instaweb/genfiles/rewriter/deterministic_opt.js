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

(function(){var a=Date,b=0,c=0,d=.462,e=1204251968254;Math.random=function(){b++;25<b&&(d+=.1,b=1);return d%1};Date=function(){if(this instanceof Date)switch(c++,25<c&&(e+=50,c=1),arguments.length){case 0:return new a(e);case 1:return new a(arguments[0]);default:return new a(arguments[0],arguments[1],3<=arguments.length?arguments[2]:1,4<=arguments.length?arguments[3]:0,5<=arguments.length?arguments[4]:0,6<=arguments.length?arguments[5]:0,7<=arguments.length?arguments[6]:0)}return(new Date).toString()};
Date.__proto__=a;Date.prototype.constructor=Date;a.now=function(){return(new Date).getTime()};})();
