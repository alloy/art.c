#include "ext.h"
#include <assert.h>
#include <dlfcn.h>
#include <ruby.h>
#include <stdio.h>

static VALUE mArtC;

/**
 * handle_event = proc do |payload, sound_palette|
 *   type = payload["type"]
 *   if type == "track"
 *     ignore_list = ["Time on page"]
 *     if !ignore_list.includes?(payload["event"])
 *       sound_palette.piano.play
 *     end
 *   elsif type == "page"
 *     sound_palette.wood_block.play
 *   elsif type == "identify"
 *     sound_palette.bell.play
 *   end
 *   nil
 * end
 */
static VALUE handle_event(RB_BLOCK_CALL_FUNC_ARGLIST(payload, sound_palette)) {
  VALUE type = rb_hash_fetch(payload, rb_str_new_cstr("type"));
  if (rb_str_equal(type, rb_str_new_cstr("track")) == Qtrue) {
    VALUE ignore_list = rb_ary_new();
    rb_ary_push(ignore_list, rb_str_new_cstr("Time on page"));
    VALUE event = rb_hash_fetch(payload, rb_str_new_cstr("event"));
    if (rb_ary_includes(ignore_list, event) == Qfalse) {
      rb_funcall(rb_funcall(sound_palette, rb_intern("piano"), 0), rb_intern("play"), 0);
      printf("EVENT TRACK: %s\n", StringValuePtr(event));
    }
  } else if (rb_str_equal(type, rb_str_new_cstr("page")) == Qtrue) {
    rb_funcall(rb_funcall(sound_palette, rb_intern("wood_block"), 0), rb_intern("play"), 0);
    VALUE properties = rb_hash_fetch(payload, rb_str_new_cstr("properties"));
    VALUE path = rb_hash_fetch(properties, rb_str_new_cstr("path"));
    VALUE path_str = rb_inspect(path);
    printf("EVENT PAGE: %s\n", StringValuePtr(path_str));
  } else if (rb_str_equal(type, rb_str_new_cstr("identify")) == Qtrue) {
    rb_funcall(rb_funcall(sound_palette, rb_intern("bell"), 0), rb_intern("play"), 0);
    VALUE traits = rb_hash_fetch(payload, rb_str_new_cstr("traits"));
    VALUE collector_level = rb_hash_fetch(traits, rb_str_new_cstr("collector_level"));
    VALUE collector_level_str = rb_inspect(collector_level);
    printf("EVENT IDENTIFY: %s\n", StringValuePtr(collector_level_str));
  }
  return Qnil;
}

/**
 * sound = ArtC::Sound.new
 *
 * piano = sound.channel(0)
 * piano.bank = 0
 *
 * wood_block = sound.channel(1)
 * wood_block.bank = 12
 *
 * bell = sound.channel(2)
 * bell.bank = 14
 *
 * SoundPalette = Struct.new(:piano, :wood_block, :bell)
 * sound_palette = SoundPalette.new(piano, wood_block, bell)
 *
 * ArtC.start_server do |payload|
 *   handle_event.call(payload, sound_palette)
 * end
 */
static void lets_dance(void) {
  VALUE cSound = rb_const_get(mArtC, rb_intern("Sound"));
  VALUE sound = rb_class_new_instance(0, NULL, cSound);

  VALUE piano = rb_funcall(sound, rb_intern("channel"), 1, INT2FIX(0));
  rb_funcall(piano, rb_intern("bank="), 1, INT2FIX(0));

  VALUE wood_block = rb_funcall(sound, rb_intern("channel"), 1, INT2FIX(1));
  rb_funcall(wood_block, rb_intern("bank="), 1, INT2FIX(12));

  VALUE bell = rb_funcall(sound, rb_intern("channel"), 1, INT2FIX(2));
  rb_funcall(bell, rb_intern("bank="), 1, INT2FIX(14));

  VALUE cSoundPalette = rb_struct_define(NULL, "piano", "wood_block", "bell", NULL);
  VALUE channels[3] = {piano, wood_block, bell};
  VALUE sound_palette = rb_class_new_instance(3, channels, cSoundPalette);

  rb_funcall_with_block(mArtC, rb_intern("start_server"), 0, NULL, rb_proc_new(handle_event, sound_palette));
}

/**
 * require "rbconfig"
 *
 * extdir = RbConfig::CONFIG["rubyarchdir"]
 * encbundle = File.join(extdir, "enc/encdb.bundle")
 * load encbundle
 */
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

/**
 * require "encoding"
 * require "bundler/setup"
 *
 * module ArtC
 * end
 *
 * require "server"
 * require "sound"
 *
 * lets_dance
 */
int main(int argc, char *argv[]) {
  ruby_init();
  ruby_init_loadpath();
  load_encoding_ext();
  rb_require("bundler/setup");

  mArtC = rb_define_module("ArtC");

  Init_ArtC_server();
  Init_ArtC_sound();

  lets_dance();

  return ruby_cleanup(0);
}
