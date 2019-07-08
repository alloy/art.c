#include <assert.h>
#include <dlfcn.h>
#include <ruby.h>
#include <stdio.h>

#define HTTP_STATUS_OK 200
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405

/**
 * This is the `app` proc implementation.
 */
static VALUE app(RB_BLOCK_CALL_FUNC_ARGLIST(env, _)) {
  VALUE request_method = rb_hash_fetch(env, rb_str_new_cstr("REQUEST_METHOD"));
  VALUE request_path = rb_hash_fetch(env, rb_str_new_cstr("PATH_INFO"));

  VALUE is_post = rb_str_equal(request_method, rb_str_new_cstr("POST"));
  VALUE matches_route = rb_str_equal(request_path, rb_str_new_cstr("/webhooks/analytics"));

  VALUE status = INT2FIX(is_post == Qfalse ? HTTP_STATUS_METHOD_NOT_ALLOWED
                                           : (matches_route == Qtrue ? HTTP_STATUS_OK : HTTP_STATUS_NOT_FOUND));
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

int main(int argc, char *argv[]) {
  ruby_init();
  ruby_init_loadpath();

  // Get location of native extensions for current Ruby
  rb_require("rbconfig");
  VALUE rb_mRbConfig = rb_const_get(rb_cObject, rb_intern("RbConfig"));
  VALUE rb_cCONFIG = rb_const_get(rb_mRbConfig, rb_intern("CONFIG"));
  VALUE extdir = rb_hash_fetch(rb_cCONFIG, rb_str_new_cstr("rubyarchdir"));
  assert(extdir != Qnil && "Failed to determine extension dir (rubyarchdir)");

  // Load Ruby encoding extension
  VALUE encbundle = rb_funcall(rb_cFile, rb_intern("join"), 2, extdir, rb_str_new_cstr("enc/encdb.bundle"));
  void *encdb = dlopen(StringValuePtr(encbundle), RTLD_NOW);
  assert(encdb != NULL && "Failed to load encdb.bundle");
  void (*Init_encdb)(void) = dlsym(encdb, "Init_encdb");
  Init_encdb();

  // Load gems
  rb_require("bundler/setup");
  rb_require("rack");

  // Rack::Handler::WEBrick
  VALUE rb_mRack = rb_const_get(rb_cObject, rb_intern("Rack"));
  VALUE rb_mRackHandler = rb_const_get(rb_mRack, rb_intern("Handler"));
  VALUE rb_cRackHandlerWEBrick = rb_const_get(rb_mRackHandler, rb_intern("WEBrick"));

  // Rack::Handler::WEBrick.run(proc { … })
  rb_funcall(rb_cRackHandlerWEBrick, rb_intern("run"), 1, rb_proc_new(app, 0));

  // int state;
  // VALUE result = rb_eval_string_protect("p Rack::Handler::WEBrick", &state);
  // if (state) {
  //   VALUE exception = rb_errinfo();
  //   rb_set_errinfo(Qnil);
  //   rb_p(exception);
  // }

  return ruby_cleanup(0);
}