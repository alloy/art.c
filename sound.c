#include <AssertMacros.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <ruby.h>

static dispatch_queue_t _queue = NULL;
static AUGraph graph = NULL;
static AudioUnit synthUnit = NULL;

enum {
  kMidiProgram_Piano = 0,
  kMidiProgram_WoodBlock = 12,
  kMidiProgram_Bell = 14,
};

// static UInt8 MidiChannelMapping[] = {kMidiProgram_Piano, kMidiProgram_WoodBlock};

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

  return result;
home:
  printf("[%s] ERROR: %d\n", __FUNCTION__, result);
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

static UInt32 middle_c = 60;
static UInt32 last_note_played = 0;

static UInt32 scale_note_to_absolute(UInt32 note) {
  switch (note) {
  case 0:
    return 0;
  case 1:
    return 2;
  case 2:
    return 4;
  case 3:
    return 5;
  case 4:
    return 7;
  case 5:
    return 9;
  case 6:
    return 11;
  default:
    return 0;
  }
}

static void play_sound_impl(unsigned long midi_channel) {
  last_note_played++;
  if (last_note_played == 7) {
    last_note_played = 0;
  }

  UInt32 noteNum = middle_c + scale_note_to_absolute(last_note_played);
  UInt32 onVelocity = 127;
  UInt32 noteOnCommand = kMidiMessage_NoteOn << 4 | midi_channel;

  // printf("Playing Note: Status: 0x%lX, Channel: %ld, Note: %ld, Vel: %ld\n", (unsigned long)noteOnCommand,
  //        (unsigned long)midi_channel, (unsigned long)noteNum, (unsigned long)onVelocity);

  OSStatus noteOnResult = MusicDeviceMIDIEvent(synthUnit, noteOnCommand, noteNum, onVelocity, 0);
  if (noteOnResult != 0) {
    printf("[%s] ERROR: %d\n", __FUNCTION__, noteOnResult);
    return;
  }

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), _queue, ^{
    OSStatus noteOffResult = MusicDeviceMIDIEvent(synthUnit, noteOnCommand, noteNum, 0, 0);
    if (noteOffResult != 0) {
      printf("[%s] ERROR: %d\n", __FUNCTION__, noteOffResult);
      return;
    }
  });
}

static VALUE play_sound(VALUE self, VALUE midi_channel) {
  unsigned long mc = FIX2ULONG(midi_channel);
  dispatch_async(_queue, ^{
    play_sound_impl(mc);
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
  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ControlChange << 4 | 0,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);
  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ProgramChange << 4 | 0, kMidiProgram_Piano, 0,
                                                0 /*sample offset*/),
                  home);

  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ControlChange << 4 | 1,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);
  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ProgramChange << 4 | 1,
                                                kMidiProgram_WoodBlock /*prog change num*/, 0, 0 /*sample offset*/),
                  home);

  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ControlChange << 4 | 2,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);
  __Require_noErr(result = MusicDeviceMIDIEvent(synthUnit, kMidiMessage_ProgramChange << 4 | 2, kMidiProgram_Bell, 0,
                                                0 /*sample offset*/),
                  home);

  // prints out the graph so we can see what it looks like...
  // CAShow(graph);

  __Require_noErr(result = AUGraphStart(graph), home);

  // create background queue from where sound will be played
  _queue = dispatch_queue_create("artc.sound", DISPATCH_QUEUE_SERIAL);

  // ok we're done now
  VALUE mArtC = rb_const_get(rb_cObject, rb_intern("ArtC"));
  rb_define_singleton_method(mArtC, "play_sound", play_sound, 1);

  return;
home:
  printf("[%s] ERROR: %d\n", __FUNCTION__, result);
  return;
}
