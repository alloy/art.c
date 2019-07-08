#include <AssertMacros.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <assert.h>
#include <dlfcn.h>
#include <ruby.h>
#include <stdio.h>

#define HTTP_STATUS_OK 200
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405

// This call creates the Graph and the Synth unit...
static OSStatus CreateAUGraph(AUGraph *outGraph, AudioUnit *outSynth) {
  OSStatus result;
  // create the nodes of the graph
  AUNode synthNode, limiterNode, outNode;

  AudioComponentDescription cd;
  cd.componentManufacturer = kAudioUnitManufacturer_Apple;
  cd.componentFlags = 0;
  cd.componentFlagsMask = 0;

  __Require_noErr(result = NewAUGraph(outGraph), home);

  cd.componentType = kAudioUnitType_MusicDevice;
  cd.componentSubType = kAudioUnitSubType_DLSSynth;

  __Require_noErr(result = AUGraphAddNode(*outGraph, &cd, &synthNode), home);

  cd.componentType = kAudioUnitType_Effect;
  cd.componentSubType = kAudioUnitSubType_PeakLimiter;

  __Require_noErr(result = AUGraphAddNode(*outGraph, &cd, &limiterNode), home);

  cd.componentType = kAudioUnitType_Output;
  cd.componentSubType = kAudioUnitSubType_DefaultOutput;
  __Require_noErr(result = AUGraphAddNode(*outGraph, &cd, &outNode), home);

  __Require_noErr(result = AUGraphOpen(*outGraph), home);

  __Require_noErr(result = AUGraphConnectNodeInput(*outGraph, synthNode, 0, limiterNode, 0), home);
  __Require_noErr(result = AUGraphConnectNodeInput(*outGraph, limiterNode, 0, outNode, 0), home);

  // ok we're good to go - get the Synth Unit...
  __Require_noErr(result = AUGraphNodeInfo(*outGraph, synthNode, 0, outSynth), home);

home:
  return result;
}
// some MIDI constants:
enum {
  kMidiMessage_ControlChange = 0xB,
  kMidiMessage_ProgramChange = 0xC,
  kMidiMessage_BankMSBControl = 0,
  kMidiMessage_BankLSBControl = 32,
  kMidiMessage_NoteOn = 0x9
};

static void play_sound(void) {
  AUGraph graph = 0;
  AudioUnit synthUnit;
  OSStatus result;
  char *bankPath = 0;

  UInt8 midiChannelInUse = 0; // we're using midi channel 1...

  // this is the only option to main that we have...
  // just the full path of the sample bank...

  // On OS X there are known places were sample banks can be stored
  // Library/Audio/Sounds/Banks - so you could scan this directory and give the user options
  // about which sample bank to use...
  // if (argc > 1)
  // 	bankPath = const_cast<char*>(argv[1]);

  __Require_noErr(result = CreateAUGraph(&graph, &synthUnit), home);

  // if the user supplies a sound bank, we'll set that before we initialize and start playing
  // if (bankPath)
  // {
  // 	FSRef fsRef;
  // 	__Require_noErr (result = FSPathMakeRef ((const UInt8*)bankPath, &fsRef, 0), home);

  // 	printf ("Setting Sound Bank:%s\n", bankPath);

  // 	__Require_noErr (result = AudioUnitSetProperty (synthUnit,
  // 										kMusicDeviceProperty_SoundBankFSRef,
  // 										kAudioUnitScope_Global, 0,
  // 										&fsRef, sizeof(fsRef)), home);

  // }

  // ok we're set up to go - initialize and start the graph
  __Require_noErr(result = AUGraphInitialize(graph), home);

  // set our bank
  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ControlChange << 4 | midiChannelInUse,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);

  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ProgramChange << 4 | midiChannelInUse,
                                                0 /*prog change num*/, 0, 0 /*sample offset*/),
                  home);

  CAShow(graph); // prints out the graph so we can see what it looks like...

  __Require_noErr(result = AUGraphStart(graph), home);

  // we're going to play an octave of MIDI notes: one a second
  for (int i = 0; i < 13; i++) {
    UInt32 noteNum = i + 60;
    UInt32 onVelocity = 127;
    UInt32 noteOnCommand = kMidiMessage_NoteOn << 4 | midiChannelInUse;

    printf("Playing Note: Status: 0x%lX, Note: %ld, Vel: %ld\n", (unsigned long)noteOnCommand, (unsigned long)noteNum,
           (unsigned long)onVelocity);

    __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, noteOnCommand, noteNum, onVelocity, 0), home);

    // sleep for a second
    usleep(1 * 1000 * 1000);

    __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, noteOnCommand, noteNum, 0, 0), home);
  }

  // ok we're done now

home:
  if (graph) {
    AUGraphStop(graph); // stop playback - AUGraphDispose will do that for us but just showing you what to do
    DisposeAUGraph(graph);
  }
  // return result;
}

/**
 * This is the `app` proc implementation.
 */
static VALUE app(RB_BLOCK_CALL_FUNC_ARGLIST(env, _)) {
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
      rb_p(json);
      play_sound();
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
  rb_require("json/ext");

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