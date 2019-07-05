#include <assert.h>
#include <dlfcn.h>
#include <ruby.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  ruby_init();
  ruby_init_loadpath();

  // Load ruby encoding lib
  void *encdb = dlopen("encdb.bundle", RTLD_NOW);
  assert(encdb != NULL && "Failed to load encdb.bundle");
  void (*Init_encdb)(void) = dlsym(encdb, "Init_encdb");
  Init_encdb();

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