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



#include "pagespeed/kernel/http/request_headers.h"

#include <map>
#include <utility>

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/headers.h"
#include "pagespeed/kernel/http/http.pb.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

class MessageHandler;

RequestHeaders::RequestHeaders() {
  Headers<HttpRequestHeaders>::SetProto(new HttpRequestHeaders);
}

void RequestHeaders::Clear() {
  Headers<HttpRequestHeaders>::Clear();
  mutable_proto()->Clear();
}

void RequestHeaders::CopyFromProto(const HttpRequestHeaders& p) {
  Headers<HttpRequestHeaders>::Clear();
  Headers<HttpRequestHeaders>::CopyProto(p);
}

void RequestHeaders::CopyFrom(const RequestHeaders& other) {
  CopyFromProto(*other.proto());
}

GoogleString RequestHeaders::ToString() const {
  GoogleString str;
  StringWriter writer(&str);
  WriteAsHttp("", &writer, NULL);
  return str;
}

// To avoid having every transitive dependency pull in the generated
// protobuf header file during compilation, we have a distinct enum
// for the RequestHeaders interface class.  We translate with switch
// statements rather than array lookups just so we don't have to bother
// initializing the array.
void RequestHeaders::set_method(Method method) {
  HttpRequestHeaders* proto = mutable_proto();
  switch (method) {
    case kOptions:     proto->set_method(HttpRequestHeaders::OPTIONS); break;
    case kGet:         proto->set_method(HttpRequestHeaders::GET);     break;
    case kHead:        proto->set_method(HttpRequestHeaders::HEAD);    break;
    case kPost:        proto->set_method(HttpRequestHeaders::POST);    break;
    case kPut:         proto->set_method(HttpRequestHeaders::PUT);     break;
    case kDelete:      proto->set_method(HttpRequestHeaders::DELETE);  break;
    case kTrace:       proto->set_method(HttpRequestHeaders::TRACE);   break;
    case kConnect:     proto->set_method(HttpRequestHeaders::CONNECT); break;
    case kPatch:       proto->set_method(HttpRequestHeaders::PATCH);   break;
    case kPurge:       proto->set_method(HttpRequestHeaders::PURGE);   break;
    case kError:       proto->set_method(HttpRequestHeaders::INVALID); break;
  }
}

RequestHeaders::Method RequestHeaders::method() const {
  switch (proto()->method()) {
    case HttpRequestHeaders::OPTIONS:     return kOptions;
    case HttpRequestHeaders::GET:         return kGet;
    case HttpRequestHeaders::HEAD:        return kHead;
    case HttpRequestHeaders::POST:        return kPost;
    case HttpRequestHeaders::PUT:         return kPut;
    case HttpRequestHeaders::DELETE:      return kDelete;
    case HttpRequestHeaders::TRACE:       return kTrace;
    case HttpRequestHeaders::CONNECT:     return kConnect;
    case HttpRequestHeaders::PATCH:       return kPatch;
    case HttpRequestHeaders::PURGE:       return kPurge;
    case HttpRequestHeaders::INVALID:     return kError;
  }
  LOG(DFATAL) << "Invalid method";
  return kGet;
}

const char* RequestHeaders::method_string() const {
  switch (proto()->method()) {
    case HttpRequestHeaders::OPTIONS:     return "OPTIONS";
    case HttpRequestHeaders::GET:         return "GET";
    case HttpRequestHeaders::HEAD:        return "HEAD";
    case HttpRequestHeaders::POST:        return "POST";
    case HttpRequestHeaders::PUT:         return "PUT";
    case HttpRequestHeaders::DELETE:      return "DELETE";
    case HttpRequestHeaders::TRACE:       return "TRACE";
    case HttpRequestHeaders::CONNECT:     return "CONNECT";
    case HttpRequestHeaders::PATCH:       return "PATCH";
    case HttpRequestHeaders::PURGE:       return "PURGE";
    case HttpRequestHeaders::INVALID:     return "ERROR";
  }
  LOG(DFATAL) << "Invalid method";
  return NULL;
}

const GoogleString& RequestHeaders::message_body() const {
  return proto()->message_body();
}

void RequestHeaders::set_message_body(const GoogleString& data) {
  mutable_proto()->set_message_body(data);
}

// Serialize meta-data to a binary stream.
bool RequestHeaders::WriteAsHttp(
    const StringPiece& url, Writer* writer, MessageHandler* handler) const {
  bool ret = true;
  GoogleString buf = StringPrintf("%s %s HTTP/%d.%d\r\n",
                                  method_string(), url.as_string().c_str(),
                                  major_version(), minor_version());
  ret &= writer->Write(buf, handler);
  ret &= Headers<HttpRequestHeaders>::WriteAsHttp(writer, handler);
  return ret;
}

bool RequestHeaders::AcceptsGzip() const {
  ConstStringStarVector v;
  if (Lookup(HttpAttributes::kAcceptEncoding, &v)) {
    for (int i = 0, nv = v.size(); i < nv; ++i) {
      StringPieceVector encodings;
      SplitStringPieceToVector(*(v[i]), ",", &encodings, true);
      for (int j = 0, nencodings = encodings.size(); j < nencodings; ++j) {
        if (StringCaseEqual(encodings[j], HttpAttributes::kGzip)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool RequestHeaders::IsXmlHttpRequest() const {
  // Check if kXRequestedWith header is present to determine whether it is
  // XmlHttpRequest or not.
  // Note: Not every ajax request sends this header but many libraries like
  // jquery, prototype and mootools etc. send this header. Google closure and
  // custom ajax hacks will not set this header.
  // It is not guaranteed that javascript present in the html loaded via
  // ajax request will execute.
  const char* x_requested_with = Lookup1(HttpAttributes::kXRequestedWith);
  if (x_requested_with != NULL &&
      StringCaseEqual(x_requested_with, HttpAttributes::kXmlHttpRequest)) {
    return true;
  }
  return false;
}

const Headers<HttpRequestHeaders>::CookieMultimap&
RequestHeaders::GetAllCookies() const {
  return *PopulateCookieMap(HttpAttributes::kCookie);
}

RequestHeaders::Properties RequestHeaders::GetProperties() const {
  Properties properties(Has(HttpAttributes::kCookie),
                        Has(HttpAttributes::kCookie2),
                        Has(HttpAttributes::kAuthorization));
  return properties;
}

bool RequestHeaders::HasCookie(StringPiece cookie_name) const {
  const CookieMultimap& cookies = GetAllCookies();
  return cookies.find(cookie_name) != cookies.end();
}

bool RequestHeaders::HasCookieValue(StringPiece cookie_name,
                                    StringPiece cookie_value) const {
  const CookieMultimap& cookies = GetAllCookies();
  typedef CookieMultimap::const_iterator Iter;
  std::pair<Iter, Iter> range = cookies.equal_range(cookie_name);
  for (Iter p = range.first; p != range.second; ++p) {
    const ValueAndAttributes& value_attr = p->second;
    if (value_attr.first == cookie_value) {
      return true;
    }
  }
  return false;
}

}  // namespace net_instaweb
