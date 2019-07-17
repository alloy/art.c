#include <AssertMacros.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <ruby.h>

static VALUE cSoundChannel;

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

#pragma mark -
#pragma mark Sound class

/**
 * The struct we will use as the Sound class' native instance variable and which holds references to the various native
 * bits we need.
 */
struct SoundData {
  AUGraph graph;
  AudioUnit synth;
  dispatch_queue_t queue;
};

/**
 * Tell Ruby the memory size of our native instance variable.
 */
static size_t sound_size(const void *data) { return sizeof(struct SoundData); }

/**
 * [No Ruby]
 *
 * The instance of the Sound class is deallocated and so should the native data it has a reference to. We’re doing that
 * from the queue to ensure that any `sound_play_impl` invocations that were still scheduled will not lead to crashes.
 *
 * We can safely release the queue right away, though, as scheduled tasks will retain their queue themselves.
 */
static void sound_free(struct SoundData *data) {
  dispatch_async(data->queue, ^{
    AUGraphStop(data->graph);
    AUGraphUninitialize(data->graph);
    AUGraphClose(data->graph);
    DisposeAUGraph(data->graph);
    free(data);
  });
  dispatch_release(data->queue);
}

/**
 * Describes the native Ruby instance variable that will hold our `struct SoundData` data.
 */
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

/**
 * module ArtC
 *   class Sound
 *     def self.allocate
 *       # [No Ruby]
 *       #
 *       # Memory is allocated for the instance data and the AudioUnit graph/Grand Central Dispatch queue that it holds
 *       # and a native Ruby instance variable `data` that holds it all is returned.
 *     end
 *   end
 * end
 */
static VALUE sound_alloc(VALUE self) {
  struct SoundData *data = malloc(sizeof(struct SoundData));
  assert(data != NULL && "Failed to allocate SoundData");

  OSStatus result;
  AUNode synthNode, limiterNode, outNode;
  AudioComponentDescription cd;

  cd.componentManufacturer = kAudioUnitManufacturer_Apple;
  cd.componentFlags = 0;
  cd.componentFlagsMask = 0;

  // Create graph
  __Require_noErr(result = NewAUGraph(&data->graph), home);

  // Add a synth
  cd.componentType = kAudioUnitType_MusicDevice;
  cd.componentSubType = kAudioUnitSubType_DLSSynth;
  __Require_noErr(result = AUGraphAddNode(data->graph, &cd, &synthNode), home);

  // Add a (volume) limiter effect
  cd.componentType = kAudioUnitType_Effect;
  cd.componentSubType = kAudioUnitSubType_PeakLimiter;
  __Require_noErr(result = AUGraphAddNode(data->graph, &cd, &limiterNode), home);

  // Add an output device
  cd.componentType = kAudioUnitType_Output;
  cd.componentSubType = kAudioUnitSubType_DefaultOutput;
  __Require_noErr(result = AUGraphAddNode(data->graph, &cd, &outNode), home);

  // 'Open' the graph
  __Require_noErr(result = AUGraphOpen(data->graph), home);

  // Connect the nodes: synth->limiter->output
  __Require_noErr(result = AUGraphConnectNodeInput(data->graph, synthNode, 0, limiterNode, 0), home);
  __Require_noErr(result = AUGraphConnectNodeInput(data->graph, limiterNode, 0, outNode, 0), home);

  // Ok we're good to go–get a reference to the synth unit
  __Require_noErr(result = AUGraphNodeInfo(data->graph, synthNode, 0, &data->synth), home);

  // Create a background queue from where MIDI events will be sent
  data->queue = dispatch_queue_create("artc.sound", DISPATCH_QUEUE_SERIAL);

  // Wrap our native Ruby instance variable and return it
  return TypedData_Wrap_Struct(self, &sound_type, data);
home:
  printf("[%s] ERROR: %d\n", __FUNCTION__, result);
  return Qnil;
}

/**
 * module ArtC
 *   class Sound
 *     def initialize
 *       # [No Ruby]
 *       #
 *       # The CoreAudio graph is initialized and started. This is all stored in a native Ruby instance variable `data`
 *       # of type `struct SoundData`.
 *     end
 *   end
 * end
 */
static VALUE sound_initialize(VALUE self) {
  struct SoundData *data;
  TypedData_Get_Struct(self, struct SoundData, &sound_type, data);

  OSStatus result;

  __Require_noErr(result = AUGraphInitialize(data->graph), home);
  __Require_noErr(result = AUGraphStart(data->graph), home);

  // prints out the graph so we can see what it looks like...
  // CAShow(graph);

  return self;
home:
  printf("[%s] ERROR: %d\n", __FUNCTION__, result);
  return Qnil;
}

/**
 * module ArtC
 *   class Sound
 *     def channel(channel, octave)
 *       Channel.new(self, channel, octave)
 *     end
 *   end
 * end
 */
static VALUE sound_get_channel(VALUE self, VALUE channel, VALUE octave) {
  VALUE argv[3] = {self, channel, octave};
  return rb_class_new_instance(3, argv, cSoundChannel);
}

/**
 * [No Ruby]
 *
 * Sends a MIDI note-on event to `channel` of the `synth` and schedules, by a slight delay, a note-off event.
 */
static void sound_play_impl(struct SoundData *data, unsigned long midi_channel, unsigned int note,
                            unsigned int velocity) {
  UInt32 noteOnCommand = kMidiMessage_NoteOn << 4 | midi_channel;

  // printf("Playing Note: Status: 0x%lX, Channel: %ld, Note: %ld, Vel: %ld\n", (unsigned long)noteOnCommand,
  //        (unsigned long)midi_channel, (unsigned long)note, (unsigned long)velocity);

  OSStatus noteOnResult = MusicDeviceMIDIEvent(data->synth, noteOnCommand, note, velocity, 0);
  if (noteOnResult != 0) {
    printf("[%s] ERROR: %d\n", __FUNCTION__, noteOnResult);
    return;
  }

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), data->queue, ^{
    OSStatus noteOffResult = MusicDeviceMIDIEvent(data->synth, noteOnCommand, note, 0, 0);
    if (noteOffResult != 0) {
      printf("[%s] ERROR: %d\n", __FUNCTION__, noteOffResult);
      return;
    }
  });
}

/**
 * module ArtC
 *   class Sound
 *     def play(channel, note, velocity)
 *       # [No Ruby]
 *       #
 *       # A pure C function is scheduled to be invoked on a background thread.
 *     end
 *   end
 * end
 */
static VALUE sound_play(VALUE self, VALUE midi_channel, VALUE note, VALUE velocity) {
  struct SoundData *data;
  TypedData_Get_Struct(self, struct SoundData, &sound_type, data);
  unsigned long c = FIX2ULONG(midi_channel);
  unsigned int n = FIX2UINT(note);
  unsigned int v = FIX2UINT(velocity);
  dispatch_async(data->queue, ^{
    sound_play_impl(data, c, n, v);
  });
  return Qnil;
}

#pragma mark -
#pragma mark Sound::Channel class

/**
 * module ArtC
 *   class Sound
 *     class Channel
 *       def initialize(sound, channel, octave)
 *         @sound, @channel, @last_played_note = sound, chanel, 0
 *         @octave_offset = (octave * 12) + 60 # middle C is at 60
 *       end
 *     end
 *   end
 * end
 */
static VALUE sound_channel_initialize(VALUE self, VALUE sound, VALUE channel, VALUE octave) {
  rb_ivar_set(self, rb_intern("sound"), sound);
  rb_ivar_set(self, rb_intern("channel"), channel);
  rb_ivar_set(self, rb_intern("last_played_note"), INT2FIX(0));

  // middle C is at 60
  VALUE octave_offset = INT2FIX((FIX2INT(octave) * 12) + 60);
  rb_ivar_set(self, rb_intern("octave_offset"), octave_offset);

  return self;
}

/**
 * module ArtC
 *   class Sound
 *     class Channel
 *       def bank=(bank)
 *         # [No Ruby]
 *         #
 *         # Send MIDI events to configure the channel of the synth to use sound from `bank`.
 *       end
 *     end
 *   end
 * end
 */
static VALUE sound_channel_set_bank(VALUE self, VALUE bank) {
  OSStatus result;

  VALUE sound = rb_ivar_get(self, rb_intern("sound"));
  int channel = FIX2INT(rb_ivar_get(self, rb_intern("channel")));

  struct SoundData *data;
  TypedData_Get_Struct(sound, struct SoundData, &sound_type, data);

  __Require_noErr(result = MusicDeviceMIDIEvent(data->synth, kMidiMessage_ControlChange << 4 | channel,
                                                kMidiMessage_BankMSBControl, 0, 0 /*sample offset*/),
                  home);
  __Require_noErr(result = MusicDeviceMIDIEvent(data->synth, kMidiMessage_ProgramChange << 4 | channel, FIX2INT(bank),
                                                0, 0 /*sample offset*/),
                  home);

  return Qnil;
home:
  printf("[%s] ERROR: %d\n", __FUNCTION__, result);
  return Qnil;
}

/**
 * module ArtC
 *   class Sound
 *     class Channel
 *       def play(velocity)
 *         @last_played_note = (@last_played_note + 1) % 7
 *         absolute_note_to_play = scale_note_to_absolute(@last_played_note) + @octave_offset
 *         @sound.play(@channel, absolute_note_to_play, velocity)
 *       end
 *     end
 *   end
 * end
 */
static VALUE sound_channel_play(VALUE self, VALUE velocity) {
  int last_played_note = FIX2INT(rb_ivar_get(self, rb_intern("last_played_note")));
  VALUE scale_note = INT2FIX((last_played_note + 1) % 7);
  rb_ivar_set(self, rb_intern("last_played_note"), scale_note);
  VALUE absolute_note = rb_funcall(self, rb_intern("scale_note_to_absolute"), 1, scale_note);

  VALUE octave_offset = rb_ivar_get(self, rb_intern("octave_offset"));
  VALUE absolute_note_to_play = INT2FIX(FIX2INT(absolute_note) + FIX2INT(octave_offset));

  VALUE sound = rb_ivar_get(self, rb_intern("sound"));
  VALUE channel = rb_ivar_get(self, rb_intern("channel"));
  rb_funcall(sound, rb_intern("play"), 3, channel, absolute_note_to_play, velocity);

  return Qnil;
}

/**
 * Takes a note in the C major scale and converts it to the absolute note.
 *
 * module ArtC
 *   class Sound
 *     class Channel
 *
 *       private
 *
 *       def scale_note_to_absolute(note)
 *         case note
 *         when 0 then 0
 *         when 1 then 2
 *         when 2 then 4
 *         when 3 then 5
 *         when 4 then 7
 *         when 5 then 9
 *         when 6 then 11
 *         else
 *           0
 *         end
 *       end
 *     end
 *   end
 * end
 */
static VALUE sound_channel_scale_note_to_absolute(VALUE self, VALUE note) {
  switch (FIX2INT(note)) {
  case 0:
    return INT2FIX(0);
  case 1:
    return INT2FIX(2);
  case 2:
    return INT2FIX(4);
  case 3:
    return INT2FIX(5);
  case 4:
    return INT2FIX(7);
  case 5:
    return INT2FIX(9);
  case 6:
    return INT2FIX(11);
  default:
    return INT2FIX(0);
  }
}

#pragma mark -
#pragma mark Initialize C extension

/**
 * module ArtC
 *   class Sound
 *     def self.allocate; end
 *     def initialize; end
 *     def play(channel, note, velocity); end
 *     def channel(channel, octave); end
 *
 *     class Channel
 *       def initialize(sound, channel, octave); end
 *       def bank=(bank); end
 *       end play(velocity); end
 *       private
 *       def scale_note_to_absolute(note); end
 *     end
 *   end
 * end
 */
void Init_ArtC_sound() {
  VALUE mArtC = rb_const_get(rb_cObject, rb_intern("ArtC"));

  VALUE cSound = rb_define_class_under(mArtC, "Sound", rb_cData);
  rb_define_alloc_func(cSound, sound_alloc);
  rb_define_method(cSound, "initialize", sound_initialize, 0);
  rb_define_method(cSound, "play", sound_play, 3);
  rb_define_method(cSound, "channel", sound_get_channel, 2);

  cSoundChannel = rb_define_class_under(cSound, "Channel", rb_cObject);
  rb_define_method(cSoundChannel, "initialize", sound_channel_initialize, 3);
  rb_define_method(cSoundChannel, "bank=", sound_channel_set_bank, 1);
  rb_define_method(cSoundChannel, "play", sound_channel_play, 1);
  rb_define_private_method(cSoundChannel, "scale_note_to_absolute", sound_channel_scale_note_to_absolute, 1);
}
