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

(function(){var orig_date = Date, random_count = 0, date_count = 0, random_seed = .462, time_seed = 1204251968254, random_count_threshold = 25, date_count_threshold = 25;
Math.random = function() {
  random_count++;
  random_count > random_count_threshold && (random_seed += .1, random_count = 1);
  return random_seed % 1;
};
Date = function() {
  if (this instanceof Date) {
    switch(date_count++, date_count > date_count_threshold && (time_seed += 50, date_count = 1), arguments.length) {
      case 0:
        return new orig_date(time_seed);
      case 1:
        return new orig_date(arguments[0]);
      default:
        return new orig_date(arguments[0], arguments[1], 3 <= arguments.length ? arguments[2] : 1, 4 <= arguments.length ? arguments[3] : 0, 5 <= arguments.length ? arguments[4] : 0, 6 <= arguments.length ? arguments[5] : 0, 7 <= arguments.length ? arguments[6] : 0);
    }
  }
  return (new Date).toString();
};
Date.__proto__ = orig_date;
Date.prototype.constructor = Date;
orig_date.now = function() {
  return (new Date).getTime();
};
})();
