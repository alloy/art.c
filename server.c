#include "ext.h"
#include <assert.h>
#include <dlfcn.h>
#include <ruby.h>
#include <stdio.h>

#define HTTP_STATUS_OK 200
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405

static VALUE mArtC;

/**
 * This is the `app` proc implementation.
 */
static VALUE app(RB_BLOCK_CALL_FUNC_ARGLIST(env, event_handler)) {
  // is_post = env["REQUEST_METHOD"] == "POST"
  VALUE request_method = rb_hash_fetch(env, rb_str_new_cstr("REQUEST_METHOD"));
  VALUE is_post = rb_str_equal(request_method, rb_str_new_cstr("POST"));
  // matches_route = env["PATH_INFO"] == "/webhooks/analytics"
  VALUE request_path = rb_hash_fetch(env, rb_str_new_cstr("PATH_INFO"));
  VALUE matches_route = rb_str_equal(request_path, rb_str_new_cstr("/webhooks/analytics"));

  int c_status = is_post == Qfalse ? HTTP_STATUS_METHOD_NOT_ALLOWED
                                   : (matches_route == Qtrue ? HTTP_STATUS_OK : HTTP_STATUS_NOT_FOUND);

  if (c_status == HTTP_STATUS_OK) {
    // json = JSON.parse(env["rack.input"].read)
    VALUE request_body_stream = rb_hash_fetch(env, rb_str_new_cstr("rack.input"));
    VALUE request_body = rb_funcall(request_body_stream, rb_intern("read"), 0);
    VALUE rb_mJSON = rb_const_get(rb_cObject, rb_intern("JSON"));
    VALUE json = rb_funcall(rb_mJSON, rb_intern("parse"), 1, request_body);

    // json["type"] == "page"
    VALUE type = rb_hash_fetch(json, rb_str_new_cstr("type"));
    if (rb_str_equal(type, rb_str_new_cstr("page")) == Qtrue) {
      rb_proc_call(event_handler, rb_ary_new3(1, json));
    }
  }

  // response = [200, {}, ["OK"]]
  VALUE status = INT2FIX(c_status);
  VALUE headers = rb_hash_new();
  VALUE body = rb_ary_new();
  rb_ary_push(body, rb_str_new_cstr("OK"));
  VALUE response = rb_ary_new();
  rb_ary_push(response, status);
  rb_ary_push(response, headers);
  rb_ary_push(response, body);
  rb_p(response);

  return response;
}

static VALUE start_server(VALUE self) {
  VALUE event_handler = rb_block_proc();

  // Rack::Handler::WEBrick
  VALUE rb_mRack = rb_const_get(rb_cObject, rb_intern("Rack"));
  VALUE rb_mRackHandler = rb_const_get(rb_mRack, rb_intern("Handler"));
  VALUE rb_cRackHandlerWEBrick = rb_const_get(rb_mRackHandler, rb_intern("WEBrick"));

  // Rack::Handler::WEBrick.run(proc { … })
  rb_funcall(rb_cRackHandlerWEBrick, rb_intern("run"), 1, rb_proc_new(app, event_handler));

  return Qnil;
}

void Init_ArtC_server(void) {
  rb_require("rack");
  rb_require("json/ext");

  mArtC = rb_const_get(rb_cObject, rb_intern("ArtC"));
  rb_define_singleton_method(mArtC, "start_server", start_server, 0);
}
