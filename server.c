#include "ext.h"
#include <ruby.h>

#define HTTP_STATUS_OK 200
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405

static VALUE mArtC;

#pragma mark -
#pragma mark Run Rack application

/**
 * app = proc do |env, event_handler|
 *   is_post = env["REQUEST_METHOD"] == "POST"
 *   matches_route = env["PATH_INFO"] == "/webhooks/analytics"
 *   status = !is_post ? HTTP_STATUS_METHOD_NOT_ALLOWED : matches_route ? HTTP_STATUS_OK : HTTP_STATUS_NOT_FOUND
 *   if status == HTTP_STATUS_OK
 *     json = JSON.parse(env["rack.input"].read)
 *     event_handler.call(event)
 *   end
 *   [status, {}, ["OK"]]
 * end
 */
static VALUE app(RB_BLOCK_CALL_FUNC_ARGLIST(env, event_handler)) {
  VALUE request_method = rb_hash_fetch(env, rb_str_new_cstr("REQUEST_METHOD"));
  VALUE is_post = rb_str_equal(request_method, rb_str_new_cstr("POST"));
  VALUE request_path = rb_hash_fetch(env, rb_str_new_cstr("PATH_INFO"));
  VALUE matches_route = rb_str_equal(request_path, rb_str_new_cstr("/webhooks/analytics"));

  int status = is_post == Qfalse ? HTTP_STATUS_METHOD_NOT_ALLOWED
                                 : (matches_route == Qtrue ? HTTP_STATUS_OK : HTTP_STATUS_NOT_FOUND);

  if (status == HTTP_STATUS_OK) {
    VALUE request_body_stream = rb_hash_fetch(env, rb_str_new_cstr("rack.input"));
    VALUE request_body = rb_funcall(request_body_stream, rb_intern("read"), 0);
    VALUE rb_mJSON = rb_const_get(rb_cObject, rb_intern("JSON"));
    VALUE event = rb_funcall(rb_mJSON, rb_intern("parse"), 1, request_body);
    rb_proc_call(event_handler, rb_ary_new3(1, event));
  }

  VALUE headers = rb_hash_new();
  VALUE body = rb_ary_new();
  rb_ary_push(body, rb_str_new_cstr("OK"));
  VALUE response = rb_ary_new();
  rb_ary_push(response, INT2FIX(status));
  rb_ary_push(response, headers);
  rb_ary_push(response, body);

  return response;
}

/**
 * def ArtC.start_server(&event_handler)
 *   Rack::Handler::WEBrick.run(proc { |env| app.call(env, event_handler) })
 * end
 */
static VALUE start_server(VALUE self) {
  VALUE event_handler = rb_block_proc();

  VALUE rb_mRack = rb_const_get(rb_cObject, rb_intern("Rack"));
  VALUE rb_mRackHandler = rb_const_get(rb_mRack, rb_intern("Handler"));
  VALUE rb_cRackHandlerWEBrick = rb_const_get(rb_mRackHandler, rb_intern("WEBrick"));

  rb_funcall(rb_cRackHandlerWEBrick, rb_intern("run"), 1, rb_proc_new(app, event_handler));

  return Qnil;
}

#pragma mark -
#pragma mark Initialize C extension

/**
 * require "rack"
 * require "json/ext"
 *
 * module ArtC
 *   def self.start_server; end
 * end
 */
void Init_ArtC_server(void) {
  rb_require("rack");
  rb_require("json/ext");

  mArtC = rb_const_get(rb_cObject, rb_intern("ArtC"));
  rb_define_singleton_method(mArtC, "start_server", start_server, 0);
}
