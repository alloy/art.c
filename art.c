#include <assert.h>
#include <dlfcn.h>
#include <ruby.h>
#include <stdio.h>

void Init_ArtC_server(VALUE);
void Init_ArtC_sound(VALUE);
static void load_bundler_env(void);
static void load_encoding_ext(void);

int main(int argc, char *argv[]) {
  ruby_init();
  ruby_init_loadpath();

  load_encoding_ext();
  load_bundler_env();

  // module ArtC; end
  VALUE mArtC = rb_define_module("ArtC");

  // Initialize other ArtC extensions
  Init_ArtC_server(mArtC);
  Init_ArtC_sound(mArtC);

  // Let's dance
  rb_funcall(mArtC, rb_intern("start_server"), 0);

  return ruby_cleanup(0);
}

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
