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


#include "pagespeed/kernel/base/json_writer.h"

#include <memory>

#include "pagespeed/kernel/base/json.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class MessageHandler;

JsonWriter::JsonWriter(Writer* writer,
                       const std::vector<ElementJsonPair>* element_json_stack)
    : writer_(writer),
      element_json_stack_(element_json_stack) {
}

JsonWriter::~JsonWriter() {
}

bool JsonWriter::Write(const StringPiece& str,
                       MessageHandler* message_handler) {
  str.AppendToString(&buffer_);
  return true;
}

void JsonWriter::UpdateDictionary() {
  Json::Value& dictionary = *(element_json_stack_->back().second);
  if (!dictionary.isMember(kInstanceHtml)) {
    dictionary[kInstanceHtml] = "";
  }
  const GoogleString& updated_instance_html =
        StrCat(dictionary[kInstanceHtml].asCString(), buffer_);
  dictionary[kInstanceHtml] = updated_instance_html.c_str();
  buffer_.clear();
}

bool JsonWriter::Flush(MessageHandler* message_handler) {
  return writer_->Flush(message_handler);
}

}  // namespace net_instaweb
