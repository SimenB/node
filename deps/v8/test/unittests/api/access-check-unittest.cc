// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using AccessCheckTest = TestWithIsolate;

namespace {

bool AccessCheck(Local<Context> accessing_context,
                 Local<Object> accessed_object, Local<Value> data) {
  return false;
}

MaybeLocal<Value> CompileRun(Isolate* isolate, const char* source) {
  Local<String> source_string =
      String::NewFromUtf8(isolate, source, NewStringType::kNormal)
          .ToLocalChecked();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Script> script =
      Script::Compile(context, source_string).ToLocalChecked();
  return script->Run(context);
}

}  // namespace

TEST_F(AccessCheckTest, GetOwnPropertyDescriptor) {
  isolate()->SetFailedAccessCheckCallbackFunction(
      [](v8::Local<v8::Object> host, v8::AccessType type,
         v8::Local<v8::Value> data) {});
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate());
  global_template->SetAccessCheckCallback(AccessCheck);

  Local<FunctionTemplate> getter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<Value>& info) { FAIL(); });
  getter_template->SetAcceptAnyReceiver(false);
  Local<FunctionTemplate> setter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<v8::Value>& info) { FAIL(); });
  setter_template->SetAcceptAnyReceiver(false);
  global_template->SetAccessorProperty(
      String::NewFromUtf8(isolate(), "property", NewStringType::kNormal)
          .ToLocalChecked(),
      getter_template, setter_template);

  Local<Context> target_context =
      Context::New(isolate(), nullptr, global_template);
  Local<Context> accessing_context =
      Context::New(isolate(), nullptr, global_template);

  accessing_context->Global()
      ->Set(accessing_context,
            String::NewFromUtf8(isolate(), "other", NewStringType::kNormal)
                .ToLocalChecked(),
            target_context->Global())
      .FromJust();

  Context::Scope context_scope(accessing_context);
  Local<Value> result =
      CompileRun(isolate(),
                 "Object.getOwnPropertyDescriptor(this, 'property')"
                 "    .get.call(other);")
          .ToLocalChecked();
  EXPECT_TRUE(result->IsUndefined());
  CompileRun(isolate(),
             "Object.getOwnPropertyDescriptor(this, 'property')"
             "    .set.call(other, 42);");
}

namespace {
bool failed_access_check_callback_called;

v8::Local<v8::String> v8_str(const char* x) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), x,
                                 v8::NewStringType::kNormal)
      .ToLocalChecked();
}

class AccessCheckTestConsoleDelegate : public debug::ConsoleDelegate {
 public:
  void Log(const debug::ConsoleCallArguments& args,
           const debug::ConsoleContext& context) {
    FAIL();
  }
};

}  // namespace

// Ensure that {console.log} does an access check for its arguments.
TEST_F(AccessCheckTest, ConsoleLog) {
  isolate()->SetFailedAccessCheckCallbackFunction(
      [](v8::Local<v8::Object> host, v8::AccessType type,
         v8::Local<v8::Value> data) {
        failed_access_check_callback_called = true;
      });
  AccessCheckTestConsoleDelegate console{};
  debug::SetConsoleDelegate(isolate(), &console);

  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetAccessCheckCallback(AccessCheck);

  Local<Context> context1 = Context::New(isolate(), nullptr);
  Local<Context> context2 = Context::New(isolate(), nullptr);

  Local<Object> object1 =
      object_template->NewInstance(context1).ToLocalChecked();
  EXPECT_TRUE(context2->Global()
                  ->Set(context2, v8_str("object_from_context1"), object1)
                  .IsJust());

  Context::Scope context_scope(context2);
  failed_access_check_callback_called = false;
  CompileRun(isolate(), "console.log(object_from_context1);").ToLocalChecked();

  ASSERT_TRUE(failed_access_check_callback_called);
}

}  // namespace v8
