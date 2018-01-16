<?php

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


header("Connection: close\r\n");
header("Cache-Control: max-age=86400");
header("Pragma: ", true);
header("Content-Type: text/css");
header("Expires: " . gmdate("D, d M Y H:i:s", time() + 86400) . " GMT");

$data = ".peachpuff {background-color: peachpuff;}" .
  "\n" .
  "." . str_pad("", 10000, uniqid()) . " {background-color: antiquewhite;}\n";

$output = "\x1f\x8b\x08\x00\x00\x00\x00\x00" .
    substr(gzcompress($data, 2), 0, -4) .
    pack('V', crc32($data)) .
    pack('V', mb_strlen($data, "latin1"));

header("Content-Encoding: gzip\r\n");
header("Content-Length: " . mb_strlen($output, "latin1"));

echo $output;
