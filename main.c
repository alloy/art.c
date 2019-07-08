#include <assert.h>
#include <dlfcn.h>
#include <ruby.h>
#include <stdio.h>

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

  int state;
  VALUE result = rb_eval_string_protect("p Rack::Handler::WEBrick", &state);
  if (state) {
    VALUE exception = rb_errinfo();
    rb_set_errinfo(Qnil);
    rb_p(exception);
  }

  return ruby_cleanup(0);
}