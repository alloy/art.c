#include <AssertMacros.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <ruby.h>

static dispatch_queue_t _queue = NULL;
static AUGraph graph = NULL;
static AudioUnit synthUnit = NULL;
static UInt8 midiChannelInUse = 0; // we're using midi channel 1...

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

static void play_sound_impl(void) {
  OSStatus result;
  UInt32 noteNum = 60;
  UInt32 onVelocity = 127;
  UInt32 noteOnCommand = kMidiMessage_NoteOn << 4 | midiChannelInUse;

  printf("Playing Note: Status: 0x%lX, Note: %ld, Vel: %ld\n", (unsigned long)noteOnCommand, (unsigned long)noteNum,
         (unsigned long)onVelocity);

  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, noteOnCommand, noteNum, onVelocity, 0), home);

  // sleep for a second
  usleep(1 * 1000 * 1000);

  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, noteOnCommand, noteNum, 0, 0), home);

home:
  return;
}

static VALUE play_sound(VALUE self) {
  dispatch_async(_queue, ^{
    play_sound_impl();
  });
  return Qnil;
}

void Init_ArtC_sound() {
  OSStatus result;

  // create graph
  __Require_noErr(result = CreateAUGraph(&graph, &synthUnit), home);

  // ok we're set up to go - initialize and start the graph
  __Require_noErr(result = AUGraphInitialize(graph), home);

  // set our bank
  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ControlChange << 4 | midiChannelInUse,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);
  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ProgramChange << 4 | midiChannelInUse,
                                                0 /*prog change num*/, 0, 0 /*sample offset*/),
                  home);

  // prints out the graph so we can see what it looks like...
  // CAShow(graph);

  __Require_noErr(result = AUGraphStart(graph), home);

  // create background queue from where sound will be played
  _queue = dispatch_queue_create("artc.sound", DISPATCH_QUEUE_SERIAL);

  // ok we're done now
  VALUE mArtC = rb_const_get(rb_cObject, rb_intern("ArtC"));
  rb_define_singleton_method(mArtC, "play_sound", play_sound, 0);

home:
  return;
}
