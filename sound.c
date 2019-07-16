#include <AssertMacros.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <ruby.h>

enum {
  kMidiProgram_Piano = 0,
  kMidiProgram_WoodBlock = 12,
  kMidiProgram_Bell = 14,
};

enum {
  kMidiMessage_ControlChange = 0xB,
  kMidiMessage_ProgramChange = 0xC,
  kMidiMessage_BankMSBControl = 0,
  kMidiMessage_BankLSBControl = 32,
  kMidiMessage_NoteOn = 0x9
};

struct SoundData {
  AUGraph graph;
  AudioUnit synth;
  dispatch_queue_t queue;
};

static void sound_free(struct SoundData *data) {
  printf("[%s] HIER!\n", __FUNCTION__);
  AUGraphStop(data->graph);
  AUGraphUninitialize(data->graph);
  AUGraphClose(data->graph);
  DisposeAUGraph(data->graph);
  dispatch_release(data->queue);
  free(data);
}

static size_t sound_size(const void *data) { return sizeof(struct SoundData); }

static const rb_data_type_t sound_type = {
    .wrap_struct_name = "sound",
    .function =
        {
            .dmark = NULL,
            .dfree = (void (*)(void *))sound_free,
            .dsize = sound_size,
        },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
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

static void play_sound_impl(struct SoundData *data, unsigned long midi_channel) {
  last_note_played++;
  if (last_note_played == 7) {
    last_note_played = 0;
  }

  UInt32 noteNum = middle_c + scale_note_to_absolute(last_note_played);
  UInt32 onVelocity = 127;
  UInt32 noteOnCommand = kMidiMessage_NoteOn << 4 | midi_channel;

  // printf("Playing Note: Status: 0x%lX, Channel: %ld, Note: %ld, Vel: %ld\n", (unsigned long)noteOnCommand,
  //        (unsigned long)midi_channel, (unsigned long)noteNum, (unsigned long)onVelocity);

  OSStatus noteOnResult = MusicDeviceMIDIEvent(data->synth, noteOnCommand, noteNum, onVelocity, 0);
  if (noteOnResult != 0) {
    printf("[%s] ERROR: %d\n", __FUNCTION__, noteOnResult);
    return;
  }

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), data->queue, ^{
    OSStatus noteOffResult = MusicDeviceMIDIEvent(data->synth, noteOnCommand, noteNum, 0, 0);
    if (noteOffResult != 0) {
      printf("[%s] ERROR: %d\n", __FUNCTION__, noteOffResult);
      return;
    }
  });
}

static VALUE play_sound(VALUE self, VALUE midi_channel) {
  struct SoundData *data;
  TypedData_Get_Struct(self, struct SoundData, &sound_type, data);
  unsigned long mc = FIX2ULONG(midi_channel);
  dispatch_async(data->queue, ^{
    play_sound_impl(data, mc);
  });
  return Qnil;
}

static VALUE sound_alloc(VALUE self) {
  struct SoundData *data = malloc(sizeof(struct SoundData));
  assert(data != NULL && "Failed to allocate SoundData");

  OSStatus result;

  // create the nodes of the graph
  AUNode synthNode, limiterNode, outNode;

  AudioComponentDescription cd;
  cd.componentManufacturer = kAudioUnitManufacturer_Apple;
  cd.componentFlags = 0;
  cd.componentFlagsMask = 0;

  __Require_noErr(result = NewAUGraph(&data->graph), home);

  cd.componentType = kAudioUnitType_MusicDevice;
  cd.componentSubType = kAudioUnitSubType_DLSSynth;

  __Require_noErr(result = AUGraphAddNode(data->graph, &cd, &synthNode), home);

  cd.componentType = kAudioUnitType_Effect;
  cd.componentSubType = kAudioUnitSubType_PeakLimiter;

  __Require_noErr(result = AUGraphAddNode(data->graph, &cd, &limiterNode), home);

  cd.componentType = kAudioUnitType_Output;
  cd.componentSubType = kAudioUnitSubType_DefaultOutput;
  __Require_noErr(result = AUGraphAddNode(data->graph, &cd, &outNode), home);

  __Require_noErr(result = AUGraphOpen(data->graph), home);

  __Require_noErr(result = AUGraphConnectNodeInput(data->graph, synthNode, 0, limiterNode, 0), home);
  __Require_noErr(result = AUGraphConnectNodeInput(data->graph, limiterNode, 0, outNode, 0), home);

  // ok we're good to go - get the Synth Unit...
  __Require_noErr(result = AUGraphNodeInfo(data->graph, synthNode, 0, &data->synth), home);

  // create background queue from where sound will be played
  data->queue = dispatch_queue_create("artc.sound", DISPATCH_QUEUE_SERIAL);

  return TypedData_Wrap_Struct(self, &sound_type, data);
home:
  printf("[%s] ERROR: %d\n", __FUNCTION__, result);
  return Qnil;
}

static VALUE sound_initialize(VALUE self) {
  struct SoundData *data;
  TypedData_Get_Struct(self, struct SoundData, &sound_type, data);

  OSStatus result;

  __Require_noErr(result = AUGraphInitialize(data->graph), home);

  // Configure channel instruments
  __Require_noErr(result = MusicDeviceMIDIEvent(data->synth, kMidiMessage_ControlChange << 4 | 0,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);
  __Require_noErr(result = MusicDeviceMIDIEvent(data->synth, kMidiMessage_ProgramChange << 4 | 0, kMidiProgram_Piano, 0,
                                                0 /*sample offset*/),
                  home);

  __Require_noErr(result = MusicDeviceMIDIEvent(data->synth, kMidiMessage_ControlChange << 4 | 1,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);
  __Require_noErr(result = MusicDeviceMIDIEvent(data->synth, kMidiMessage_ProgramChange << 4 | 1,
                                                kMidiProgram_WoodBlock /*prog change num*/, 0, 0 /*sample offset*/),
                  home);

  __Require_noErr(result = MusicDeviceMIDIEvent(data->synth, kMidiMessage_ControlChange << 4 | 2,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);
  __Require_noErr(result = MusicDeviceMIDIEvent(data->synth, kMidiMessage_ProgramChange << 4 | 2, kMidiProgram_Bell, 0,
                                                0 /*sample offset*/),
                  home);

  // prints out the graph so we can see what it looks like...
  // CAShow(graph);

  __Require_noErr(result = AUGraphStart(data->graph), home);

  return self;
home:
  printf("[%s] ERROR: %d\n", __FUNCTION__, result);
  return Qnil;
}

void Init_ArtC_sound() {
  // ok we're done now
  VALUE mArtC = rb_const_get(rb_cObject, rb_intern("ArtC"));
  VALUE cSound = rb_define_class_under(mArtC, "Sound", rb_cData);
  rb_define_alloc_func(cSound, sound_alloc);
  rb_define_method(cSound, "initialize", sound_initialize, 0);
  rb_define_method(cSound, "play_sound", play_sound, 1);
}
