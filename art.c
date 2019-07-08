#include "ext.h"
#include <assert.h>
#include <dlfcn.h>
#include <ruby.h>
#include <stdio.h>

static VALUE mArtC;

static void load_bundler_env(void) { rb_require("bundler/setup"); }

static void load_encoding_ext(void) {
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
}

static VALUE handle_event(RB_BLOCK_CALL_FUNC_ARGLIST(event, _)) {
  rb_p(event);
  VALUE type = rb_hash_fetch(event, rb_str_new_cstr("type"));
  // event["type"] == "track"
  if (rb_str_equal(type, rb_str_new_cstr("track")) == Qtrue) {
    rb_funcall(mArtC, rb_intern("play_sound"), 0);
  }
  // event["type"] == "page"
  else if (rb_str_equal(type, rb_str_new_cstr("page")) == Qtrue) {
    rb_funcall(mArtC, rb_intern("play_sound"), 0);
  }
  return Qnil;
}

int main(int argc, char *argv[]) {
  ruby_init();
  ruby_init_loadpath();

  load_encoding_ext();
  load_bundler_env();

  // module ArtC; end
  mArtC = rb_define_module("ArtC");

  // Initialize other ArtC extensions
  Init_ArtC_server();
  Init_ArtC_sound();

  // Let's dance

  // ArtC.start_server { … }
  rb_funcall_with_block(mArtC, rb_intern("start_server"), 0, NULL, rb_proc_new(handle_event, 0));

  return ruby_cleanup(0);
}
