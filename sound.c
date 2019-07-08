#include <AssertMacros.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <ruby.h>

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

void play_sound(void) {
  AUGraph graph = 0;
  AudioUnit synthUnit;
  OSStatus result;
  char *bankPath = 0;

  UInt8 midiChannelInUse = 0; // we're using midi channel 1...

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

  // prints out the graph so we can see what it looks like...
  // CAShow(graph);

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

void Init_ArtC_sound(VALUE mArtC) {}