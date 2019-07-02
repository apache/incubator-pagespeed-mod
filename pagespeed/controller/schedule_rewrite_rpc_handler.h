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


#ifndef PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_HANDLER_H_
#define PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_HANDLER_H_

#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/controller.pb.h"
#include "pagespeed/controller/request_result_rpc_handler.h"
#include "pagespeed/controller/rpc_handler.h"
#include "pagespeed/controller/schedule_rewrite_controller.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/util/grpc.h"

namespace net_instaweb {

// RpcHandler for ExpensiveOperationController.
//
// The request message on the RPC contains the key that the client wants to
// rewrite. This will trigger a call to HandleClientRequest() which we use to
// call ScheduleRewrite(). When the controller decides if it will allow the
// rewrite to proceed, RequestResultRpcHandler returns that decision to the
// client. Once the client completes, it sends another Request message
// indicating success or failure, which will trigger a call to
// HandleClientResult() which we then dispatch to NotifyRewriteComplete()
// or NotifyRewriteFailed().
//
// If the client disconnects after requesting an rewrite but before sending a
// second "completed" message, we receive a call to HandleOperationFailed() and
// will call NotifyRewriteFailed() on the controller, so it can release "locks".

class ScheduleRewriteRpcHandler
    : public RequestResultRpcHandler<
          ScheduleRewriteRpcHandler, ScheduleRewriteController,
          grpc::CentralControllerRpcService::AsyncService,
          ScheduleRewriteRequest, ScheduleRewriteResponse> {
 protected:
  ScheduleRewriteRpcHandler(
      grpc::CentralControllerRpcService::AsyncService* service,
      ::grpc::ServerCompletionQueue* cq, ScheduleRewriteController* controller);

  // RequestResultRpcHandler implementation.
  void HandleClientRequest(const ScheduleRewriteRequest& req,
                           Function* cb) override;
  void HandleClientResult(const ScheduleRewriteRequest& req) override;
  void HandleOperationFailed() override;

  void InitResponder(grpc::CentralControllerRpcService::AsyncService* service,
                     ::grpc::ServerContext* ctx, ReaderWriterT* responder,
                     ::grpc::ServerCompletionQueue* cq,
                     void* callback) override;

 private:
  GoogleString key_;  // What we told the controller that we're rewriting.

  // Allow access to protected constructor.
  friend class RequestResultRpcHandler;
  friend class ScheduleRewriteRpcHandlerTest;

  DISALLOW_COPY_AND_ASSIGN(ScheduleRewriteRpcHandler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_HANDLER_H_
