/*
 * FluidSynth VST
 *
 * Wrapper part (this file): Copyright (C) 2019 AZ (www.azslow.com)
 *
 * FluidSynth: Copyright (C) 2003-2019  Peter Hanappe and others.
 * VST API: (c) 2019, Steinberg Media Technologies GmbH
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <algorithm>

#include "public.sdk/source/main/pluginfactory.h"
#include "base/source/fstreamer.h"
#include "base/source/fbuffer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"

#include "../include/fluidsynthvst.h"

#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "pluginterfaces/vst/ivstattributes.h"

namespace FluidSynthVST {
using namespace Steinberg;


#ifdef WIN32
#else /* Linux */
#include <time.h>

class PerfMeter {
  private:
    struct timespec start_ts;
    struct timespec end_ts;
    unsigned long long md;
    char *pfx;

  public:
    PerfMeter(const char *prefix = "Perf", unsigned long long min_diff = 1) : md(min_diff){
      clock_gettime(CLOCK_REALTIME, &start_ts);
      pfx = strdup(prefix);
    }
    ~PerfMeter(){
      clock_gettime(CLOCK_REALTIME, &end_ts);
      unsigned long long td = (end_ts.tv_sec - start_ts.tv_sec)*1000000 + (end_ts.tv_nsec - start_ts.tv_nsec)/1000;
      if(td >= md)
	printf("%s: %llu uSec\n", pfx, td);
    }
};
#endif /* platform */

// CC Names
static const char *szCCName[Vst::kCountCtrlNumber] = {
    // MSB
    "Bank (MSB)", //= 0x00
    "Modulation (MSB)", //= 0x01,
    "Breath (MSB)", //= 0x02,
    "",
    "Foot (MSB)", //= 0x04
    "Portamento time (MSB)", //= 0x05,
    "Data entry (MSB)", //= 0x06,
    "Volume (MSB)", //= 0x07,
    "Balance (MSB)", //= 0x08,
    "",
    "Pan (MSB)", //= 0x0A,
    "Expression (MSB)", //= 0x0B,
    "Effect Control 1 (MSB)", //= 0x0C,
    "Effect Control 2 (MSM)", //= 0x0D,
    "",
    "",
    "General 1 (MSB)", //= 0x10,
    "General 2 (MSB)", //= 0x11,
    "General 3 (MSB)", //= 0x12,
    "General 4 (MSB)", //= 0x13,
    "", "", "", "", "", "", "", "", "", "", "", "", // 12x

    // LSB
    "Bank (LSB)", //= 0x20
    "Modulation (LSB)", //= 0x21,
    "Breath (LSB)", //= 0x22,
    "",
    "Foot (LSB)", //= 0x24
    "Portamento time (LSB)", //= 0x25,
    "Data entry (LSB)", //= 0x26,
    "Volume (LSB)", //= 0x27,
    "Balance (LSB)", //= 0x28,
    "",
    "Pan (LSB)", //= 0x2A,
    "Expression (LSB)", //= 0x2B,
    "Effect Control 1 (LSB)", //= 0x2C,
    "Effect Control 2 (LSm)", //= 0x2D,
    "",
    "",
    "General 1 (LSB)", //= 0x30,
    "General 2 (LSB)", //= 0x31,
    "General 3 (LSB)", //= 0x32,
    "General 4 (LSB)", //= 0x33,
    "", "", "", "", "", "", "", "", "", "", "", "", // 12x

    // Switches
    "Sustain", //= 0x40
    "Portamento", //= 0x41
    "Sostenuto", //= 0x42
    "Soft pedal", //= 0x43
    "Legato", //= 0x44
    "Hold 2 ", //= 0x45

    // 7bit controllers
    "Sound controller 1", //=0x46
    "Sound controller 2", //=0x47
    "Sound controller 3", //=0x48
    "Sound controller 4", //=0x49
    "Sound controller 5", //=0x4a
    "Sound controller 6", //=0x4b
    "Sound controller 7", //=0x4c
    "Sound controller 8", //=0x4d
    "Sound controller 9", //=0x4e
    "Sound controller 10", //=0x4f
    "General 5", //= 0x50
    "General 6", //= 0x51
    "General 7", //= 0x52
    "General 8", //= 0x53
    "Portamento control", //= 0x54
    "", "", "", "", "", "", // 6x
    "Effects depth 1 (Reverb)", //= 0x5B
    "Effects depth 2", //= 0x5C
    "Effects depth 3 (Chorus)", //= 0x5D
    "Effects depth 4", //= 0x5E
    "Effects depth 5", //= 0x5F

    // System
    "Increment", //=0x60
    "Decrement", //=0x61
    "NRPN (LSB)", //=0x62
    "NRPN (MSB)", //= 0x63
    "RPN (LSB)", //=0x64
    "RPN (MSB)", //= 0x65
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 18x
    "All sound off", //= 0x78
    "Reset controllers", //= 0x79
    "Local control", //= 0x7A
    "All notes off", //= 0x7B
    "Omni off", //= 0x7C
    "Onmi on", //= 0x7D
    "Mono mode on", //= 0x7E
    "Poly mode on", //= 0x7F

    // VST extra
    "AT", //= 0x80
    "PB", //= 0x81
};


// Processor
Processor::Processor() : mSynth(NULL), mChangeSoundFont(false), mLoadingThread(0) /*, mAudioBufsSize(0) */ {
  setControllerClass(ControllerUID);
  mSynthSettings = new_fluid_settings();
  mSynth = new_fluid_synth(mSynthSettings);

  scanSoundFonts();

  /*
  mAudioBufs[0] = NULL;
  mAudioBufs[1] = NULL;
  */
}

void Processor::syncedLoadSoundFont() {
  char fileName[FILENAME_MAX];
  GetPath(fileName, FILENAME_MAX);
  PathAppend(fileName, FILENAME_MAX, mLoadedFile.text8());

  fluid_synth_all_notes_off(mSynth, -1);
  fluid_synth_all_sounds_off(mSynth, -1);
  if(fluid_synth_sfcount(mSynth) > 0){
    // unload old one
    int id = fluid_sfont_get_id(fluid_synth_get_sfont(mSynth, 0));
    fluid_synth_sfunload(mSynth, id, 1);
  }
  if((mSoundFontID = fluid_synth_sfload(mSynth, fileName, 1)) == FLUID_FAILED){
    printf("Failed '%s'...\n", fileName);
  }
  mLoadingComplete = true;
}

Processor::~Processor() {
  checkSoundFont(true); // to stop the thread, if any
  if(mSynth){
    delete_fluid_synth(mSynth);
    mSynth = NULL;
  }
  if(mSynthSettings){
    delete_fluid_settings(mSynthSettings);
    mSynthSettings = NULL;
  }
  /*
  if(mAudioBufs[0])
    delete [] mAudioBufs[0];
  if(mAudioBufs[1])
    delete [] mAudioBufs[1];
    */
}

tresult PLUGIN_API Processor::initialize(FUnknown* context){
  tresult result = AudioEffect::initialize(context);
  if(result == kResultTrue){
    addAudioInput(STR16("AudioInput"), Vst::SpeakerArr::kStereo);
    addAudioOutput(STR16("AudioOutput"), Vst::SpeakerArr::kStereo);
  }
  return result;
}

tresult PLUGIN_API Processor::setBusArrangements(Vst::SpeakerArrangement* inputs, int32 numIns,
						 Vst::SpeakerArrangement* outputs, int32 numOuts){
  if((numIns == 1) && (numOuts == 1) && (inputs[0] == outputs[0])){
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
  }
  return kResultFalse;
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize){
  if(symbolicSampleSize == Vst::kSample32)
    return kResultTrue;
  // printf("64bit (%d) audio is not supported by now\n", symbolicSampleSize);
  // Well, FluidSynth has compile time fixed precision. And it is default to 64bit... But
  // fluid_synth_write_float unconditionally writes "float", so 32bit...
  return kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(Vst::ProcessSetup& setup){
  tresult result = AudioEffect::setupProcessing(setup);
  if(result == kResultTrue){
    //printf("Processor: SetupProcessing\n");
    // TODO: set block size, etc.
    if(fluid_settings_setnum(mSynthSettings, "synth.sample-rate", setup.sampleRate) == FLUID_FAILED){
      printf("Could not set sample rate to %f\n", setup.sampleRate);
    }
    // BAD SDK:
    //   from common sense, we should not load any sound font till we know which one should be loaded
    //   but reality is different. REAPER calls setupProcessing before setState, even in case it is
    //   called later (the plug-in is instantiated from a project, from saved state).
    //   In case last getState returns the same as default getState, setState is not called at all.
    //   Finally, if plug-in is just instantiated, setState should not be called. And so,
    //   we have no way to check the user wants not default font in this instance, we are forced
    //   to always load default first.
    if(!mSoundFontFile.text8()[0]){
      mSoundFontFile = mSoundFontFiles.at(getCurrentSoundFontIdx()); // we know the list is not emply
      mChangeSoundFont = true;
    }
    checkSoundFont(true);
    /*
    if((setup.sampleRate == Vst::kSample64) && (mAudioBufsSize < setup.maxSamplesPerBlock)){
      // we should be called with real time stopped
      if(mAudioBufs[0])
	delete [] mAudioBufs[0];
      if(mAudioBufs[1])
	delete [] mAudioBufs[1];
      mAudioBufs[0] = new float[setup.maxSamplesPerBlock + 1];
      mAudioBufs[1] = new float[setup.maxSamplesPerBlock + 1];
      if(mAudioBufs[0] && mAudioBufs[1])
	mAudioBufsSize = setup.maxSamplesPerBlock;
      else
	mAudioBufsSize = 0;
      }
      */
  }
  return result;
}

tresult PLUGIN_API Processor::setActive(TBool state){
  tresult result = AudioEffect::setActive(state);
  if(result == kResultTrue){
    if(state){
      //printf("Processor: activated\n");
    } else {
      //printf("Processor: deactivated\n");
    }
  }
  return result;
}

void Processor::writeAudio(Vst::ProcessData& data, int32 start_sample, int32 end_sample){ // end_sample is exclusive
  if((data.numSamples < end_sample) || (start_sample >= end_sample) || (data.numOutputs < 1) || (data.outputs[0].numChannels < 2))
    return;

  if(data.symbolicSampleSize == Vst::kSample32){
    if(!checkSoundFont(false)){
      // the synth is not ready
      memset(data.outputs[0].channelBuffers32[0] + start_sample, 0, sizeof(float) * (end_sample - start_sample));
      memset(data.outputs[0].channelBuffers32[1] + start_sample, 0, sizeof(float) * (end_sample - start_sample));
    } else {
      if(fluid_synth_write_float(mSynth, end_sample - start_sample,
				data.outputs[0].channelBuffers32[0] + start_sample, 0, 1,
				data.outputs[0].channelBuffers32[1] + start_sample, 0, 1) == FLUID_FAILED){
	//printf("Generation failed\n");
      }
    }
  } else {
    // MAYBE TODO: handle 64bit audio case
    // fluid_synth_write_double does not exist (yet)
    // I have started to add tmp buffer, but I think hosts can handle that limitation automatically
  }
}

// A bit not optimal...

// return earliest change/event sample offset after (strict) curSample (which can be -1)
// return -1 in case there is no changes/event after curSample
// Can it return offset greater or equal data.numSamples? I try to process that correctly, just in case...
int32 Processor::nextOffset(Vst::ProcessData& data, int32 curSample){
  int32 minSampleOffset = -1;
  if(data.inputParameterChanges){
    int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
    for(int32 index = 0; index < numParamsChanged; index++){
      Vst::IParamValueQueue* paramQueue = data.inputParameterChanges->getParameterData(index);
      if(paramQueue){
	Vst::ParamValue value;
	int32 sampleOffset;
	int32 numPoints = paramQueue->getPointCount();
	for(int32 point = 0; point < numPoints; ++point){
	  if(paramQueue->getPoint(point, sampleOffset, value) == kResultTrue){
	    if((minSampleOffset >= 0) && (sampleOffset > minSampleOffset))
	      break; // assume they are time ordered
	    if(sampleOffset > curSample)
	      minSampleOffset = sampleOffset;
	  }
	}
      }
    }
  }
  Vst::Event e;
  int32 evcount = data.inputEvents ? data.inputEvents->getEventCount() : 0;
  for(int32 index = 0; index < evcount; ++index){
    if(data.inputEvents->getEvent(index, e) == kResultTrue){
      if((minSampleOffset >= 0) && (e.sampleOffset > minSampleOffset))
	break; // assume they are time ordered
      if(e.sampleOffset > curSample)
	minSampleOffset = e.sampleOffset;
    }
  }
  return minSampleOffset;
}

void Processor::playParChanges(Vst::ProcessData& data, int32 curSample, int32 endSample){ // endSample inclusive
  if(!data.inputParameterChanges)
    return;
  int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
  for(int32 index = 0; index < numParamsChanged; index++){
    Vst::IParamValueQueue* paramQueue = data.inputParameterChanges->getParameterData(index);
    if(paramQueue){
      Vst::ParamValue value;
      int32 sampleOffset;
      Vst::ParamID id = paramQueue->getParameterId();
      int32 numPoints = paramQueue->getPointCount();
      for(int32 point = 0; point < numPoints; ++point){
	if(paramQueue->getPoint(point, sampleOffset, value) == kResultTrue){
	  if(sampleOffset < curSample)
	    continue; // already played
	  if(sampleOffset > endSample)
	    break; // assume they are time ordered
	  switch(id){
	    case FluidSynthVSTParams::kBypassId:
	      mBypass = (value > 0.5f);
	      break;
	    case FluidSynthVSTParams::kRootPrgId:
	      // we can not change here, but we schedule the change on restart
	      if(mSoundFontFiles.size()){
		int32 soundFontIdx = (value*(mSoundFontFiles.size()-1) + 0.5);
		//printf("Processor: font change request\n");
		if(mSoundFontFile != mSoundFontFiles.at(soundFontIdx)){
		  mSoundFontFile = mSoundFontFiles.at(soundFontIdx);
		  mChangeSoundFont = true;
		  //printf("Processor: will change font to %s\n", mSoundFontFile.text8());
		  checkSoundFont(false);
		}
	      }
	      break;
	    default:
	      if(!checkSoundFont(false)){
		// the synth is not ready
		break;
	      }
	      if(id >= 1024){
		int32 ch = id / 1024 - 1;
	        int32 ctrlNumber = id%1024;
	        if(ctrlNumber < Vst::kAfterTouch){ // CC
		  fluid_synth_cc(mSynth, ch, ctrlNumber, value*127.+0.5);
		  //printf("Ch:%d CC%d = %d\n", ch, ctrlNumber, (int)(value*127. + 0.5));
		} else if(ctrlNumber == Vst::kAfterTouch){
		  fluid_synth_channel_pressure(mSynth, ch, value*127.+0.5);
		  //printf("Ch:%d AT = %d\n", ch, (int)(value*127. + 0.5));
		} else if(ctrlNumber == Vst::kPitchBend){
		  fluid_synth_pitch_bend(mSynth, ch, value*16383.+0.5);
		  //printf("Ch:%d PB = %d\n", ch, ((int)(value*16383. + 0.5)) - 8192);
		} else {
		  printf("Hmm... unknown control %d\n", ctrlNumber);
		}
	      } else if((id >= kChPrgId) && (id <= kLastChPrgId)){ // PC
		fluid_synth_program_change(mSynth, id - kChPrgId, value*127.+0.5);
	      } else {
		printf("Unknown param change ID: %d\n", id);
	      }
	      // TODO: also send as "legacy MIDI events"
	  }
	}
      }
    }
  }
}

void Processor::playEvents(Vst::ProcessData& data, int32 curSample, int32 endSample){ // endSample inclusive
  Vst::Event e;
  int32 evcount = data.inputEvents ? data.inputEvents->getEventCount() : 0;
  for(int32 index = 0; index < evcount; ++index){
    if(data.inputEvents->getEvent(index, e) == kResultTrue){
      if(e.sampleOffset < curSample)
	continue; // already played
      if(e.sampleOffset > endSample)
	break; // assume they are time ordered
      switch(e.type){
	case Vst::Event::kNoteOnEvent:
	  if(fluid_synth_noteon(mSynth, e.noteOn.channel, e.noteOn.pitch, e.noteOn.velocity*127. + 0.5) == FLUID_FAILED){
	    //printf("NoteOn failed\n");
	  }
	  break;
	case Vst::Event::kNoteOffEvent:
	  fluid_synth_noteoff(mSynth, e.noteOff.channel, e.noteOff.pitch);
	  break;
	default:
	  // TODO: at least SysEx
	  printf("Unprocessed Event type: %d\n", e.type);
      }
      if(data.outputEvents)
        data.outputEvents->addEvent(e);
    }
  }
}

tresult PLUGIN_API Processor::process(Vst::ProcessData& data){
  //PerfMeter pm("Process", 8000);
  //printf("*\n");

  if((data.numOutputs <= 0) || (data.numSamples <= 0))
    return kResultOk;

  int32 curSample = -1;
  while(true){
    int32 offset = nextOffset(data, curSample);
    int32 sample = (curSample < 0 ? 0 : curSample);
    int32 endSample = data.numSamples;
    if((offset >= 0) && (offset < endSample)){
      // write before offset, which is withing the block
      endSample = offset;
    }
    if(endSample > sample)
      writeAudio(data, sample, endSample);
    if(offset >= 0){
      playParChanges(data, curSample, endSample);
      playEvents(data, curSample, endSample);
    }
    curSample = endSample;
    if((curSample >= data.numSamples) && (offset < 0))
      break;
  }
  return kResultOk;
}

tresult PLUGIN_API Processor::setProcessing (TBool state){
  if(state){
    //printf("Processor: started\n");
  } else {
    //printf("Processor: ended\n");
  }
  return kResultTrue;
}

int Processor::getCurrentSoundFontIdx(){
  if(mSoundFontFiles.size() < 2)
    return 0;
  auto it = std::find(mSoundFontFiles.begin(), mSoundFontFiles.end(), mSoundFontFile);
  if(it == mSoundFontFiles.end()){ // can be if not set or not exist, return default
    auto dit = std::find(mSoundFontFiles.begin(), mSoundFontFiles.end(), "default.sf2");
    if(dit == mSoundFontFiles.end())
      return 0.; // the first
    return dit - mSoundFontFiles.begin();
  }
  return it - mSoundFontFiles.begin();
}

float Processor::getCurrentSoundFontNormalized(){
  return (float)(getCurrentSoundFontIdx()) / (mSoundFontFiles.size() - 1);
}


tresult PLUGIN_API Processor::setState(IBStream* state){
  if(!state)
    return kResultFalse;

  //printf("Processor: setState\n");

  IBStreamer streamer(state, kLittleEndian);

  int32 savedBypass = 0;
  if(streamer.readInt32(savedBypass) == false)
    return kResultFalse;
  mBypass = savedBypass > 0;

  String newSoundFontFile;
  char *soundFontFileCStr;
  if(!(soundFontFileCStr = streamer.readStr8())){
    // can be, the first version was not saving it
    newSoundFontFile = "";
    //printf("Processor: using default font\n");
  } else {
    newSoundFontFile = soundFontFileCStr;
    delete soundFontFileCStr;
    //printf("Processor: State font: %s\n", mSoundFontFile.text8());
  }
  if(!newSoundFontFile.text8()[0])
    newSoundFontFile = mSoundFontFiles.at(getCurrentSoundFontIdx()); // the list is not empty after scanning
  if(newSoundFontFile != mSoundFontFile){
    mSoundFontFile = newSoundFontFile;
    auto it = std::find(mSoundFontFiles.begin(), mSoundFontFiles.end(), mSoundFontFile);
    if(it == mSoundFontFiles.end()){
      mSoundFontFiles.push_back( mSoundFontFile ); // some not existing sound font, add it
      std::sort(mSoundFontFiles.begin(), mSoundFontFiles.end());
      sendProgramList();
    }
    mChangeSoundFont = true;
    sendCurrentProgram();
  }
  return kResultOk;
}

tresult PLUGIN_API Processor::getState(IBStream* state){
  int32 toSaveBypass = mBypass ? 1 : 0;

  //printf("Processor: getState\n");

  IBStreamer streamer(state, kLittleEndian);
  streamer.writeInt32(toSaveBypass);
  streamer.writeStr8(mSoundFontFile.text8()); // can be empty
  //printf("   Current sound font: %s\n", mSoundFontFile.text8());

  // in case there will be no future setState, controller will be called with this state
  // but "empty" font effectively means "default.sf2", in case it exist. And it can be not the first one.
  sendCurrentProgram();

  return kResultOk;
}

void Processor::sendProgramList(){
  Steinberg::Buffer buf;
  for(auto const& fileName : mSoundFontFiles){
    String name;
    fileName.extract(name, 0, fileName.length() - 4); // remove ".sfX"
    buf.appendString8(name); // UTF-8
    buf.endString8();
  }
  Vst::IMessage* message = allocateMessage();
  FReleaser msgReleaser(message);
  if(message){
    message->setMessageID("SoundFontFiles");
    message->getAttributes()->setBinary("List", buf, buf.getFillSize());
    //printf("  sending %d, first: %s\n", buf.getFillSize(), buf.str8());
    sendMessage(message);
  }
}

tresult PLUGIN_API Processor::connect (IConnectionPoint* other){
  if(Vst::AudioEffect::connect(other) == kResultOk){
    //printf("Processor: connected\n");
    sendProgramList();
  }
  return kResultFalse;
}

void Processor::sendCurrentProgram(){
  float cSoundFontNorm = getCurrentSoundFontNormalized();
  Vst::IMessage* message = allocateMessage();
  FReleaser msgReleaser(message);
  if(message){
    message->setMessageID("CurrentSoundFont");
    message->getAttributes()->setBinary("Value", &cSoundFontNorm, sizeof(float));
    sendMessage(message);
  }
}

// Controller


tresult PLUGIN_API Controller::initialize(FUnknown* context){
  tresult result = EditController::initialize(context);
  if(result != kResultOk){
    return result;
  }

  parameters.addParameter(STR16("Bypass"), nullptr, 1, 0,
			  Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsBypass,
			  FluidSynthVSTParams::kBypassId);

  addUnit(new Vst::Unit(String("Root"), Vst::kRootUnitId, Vst::kNoParentUnitId, kRootPrgId /* Vst::kNoProgramListId */));

  // register so far empty Sound Font list and its parameter
  Vst::ProgramList* prgList = new Vst::ProgramList(String("Sound Font"), kRootPrgId, Vst::kRootUnitId);
  addProgramList(prgList);
  prgList->addProgram(String("default")); // something crash otherwise...
  mCurrentProgram = 0.; // we will be informed about real one

  Vst::Parameter* prgParam = prgList->getParameter();
  prgParam->getInfo().flags &= ~Vst::ParameterInfo::kCanAutomate;
  parameters.addParameter(prgParam);


  for(int32 ch = 0; ch < 16; ++ch){
    Vst::UnitID unitId = ch + 1;
    Vst::ProgramListID prgListId = kChPrgId + ch;
    String unitName;
    // Unit
    unitName.printf("Ch%d", ch + 1);
    addUnit(new Vst::Unit(unitName, unitId, Vst::kRootUnitId, prgListId /* Vst::kNoProgramListId */));
    // ProgramList
    String listName;
    listName.printf("Ch%d", ch + 1);
    Vst::ProgramList* prgList = new Vst::ProgramList(listName, prgListId, unitId);
    addProgramList(prgList);
    for(int32 i = 0; i < 128; i++){
      String title;
      title.printf("Prog %d", i);
      prgList->addProgram(title);
    }
    // ProgramList parameter
    Vst::Parameter* prgParam = prgList->getParameter();
    prgParam->getInfo().flags &= ~Vst::ParameterInfo::kCanAutomate;
    parameters.addParameter(prgParam);
    /*
    unitName.printf("Prg Ch%d", ch + 1);
    parameters.addParameter(unitName, nullptr, 127, 0,
                    Vst::ParameterInfo::kIsProgramChange | Vst::ParameterInfo::kIsList,
                    prgListId, unitId);
		    */

    // CC, (Channel) AfterTouch, PitchBend
    for(int midiCtrlNumber = 0; midiCtrlNumber < Vst::kCountCtrlNumber; ++midiCtrlNumber){
      String parName;
      int nSteps;
      if(!szCCName[midiCtrlNumber][0])
	continue;
      parName.printf("Ch:%d %s", ch + 1, szCCName[midiCtrlNumber]);
      if(midiCtrlNumber ==Vst::kPitchBend){
	nSteps = 128*128 - 1; // 16383
      } else {
	nSteps = 127;
      }
      parameters.addParameter(parName, nullptr, nSteps, 0, Vst::ParameterInfo::kNoFlags, 1024 + 1024*ch + midiCtrlNumber);
    }
  }
  return kResultOk;
}

tresult PLUGIN_API Controller::notify (Vst::IMessage* message){
  if(!message)
    return kInvalidArgument;
  //printf("Controller: Notify\n");
  FIDString messageID = message->getMessageID();
  uint32 messageSize = 0;
  const char *messageData = NULL;
  if(!strcmp(messageID, "SoundFontFiles")){
    //printf(" SoundFontFiles\n");
    if(message->getAttributes()->getBinary("List", (const void *&)messageData, messageSize) == kResultOk){
      // that should be an array of UTF8 strings
      if(messageData && messageSize && !messageData[messageSize - 1]){
	auto prgList = getProgramList(kRootPrgId);
	auto prgPar  = dynamic_cast<Vst::StringListParameter *>(prgList->getParameter());
	if(prgList && prgPar){
	  const char *messageEnd = messageData + messageSize;
	  // BAD SDK: there is no call to clear the list, so we can only replace...
	  int32 currentProgramCount = prgList->getCount();
	  int idx = 0;
	  // printf(" Controller: font list\n");
	  while(messageData < messageEnd){
	    String soundFont;
	    soundFont.fromUTF8(messageData);
	    // printf("  %s\n", soundFont.text8());
	    if(idx >= currentProgramCount){
	      prgList->addProgram(soundFont);
	      prgPar->appendString(soundFont);
	    } else {
	      prgList->setProgramName(idx, soundFont);
	      prgPar->replaceString(idx, soundFont);
	    }
	    messageData += strlen(messageData) + 1;
	    ++idx;
	  }
	  // TODO: set current value for parameter
	  if(componentHandler)
	    componentHandler->restartComponent(Vst::kParamValuesChanged);
	  notifyProgramListChange(kRootPrgId); // Ask host to redraw possible "build-in presets"
	  return kResultTrue;
	}
      }
    }
  } else if(!strcmp(messageID, "CurrentSoundFont")){
    //printf(" CurrentSoundFont\n");
    if(message->getAttributes()->getBinary("Value", (const void *&)messageData, messageSize) == kResultOk){
      // that should be an array of UTF8 strings
      if(messageData && (messageSize == sizeof(float))){
	memcpy(&mCurrentProgram, messageData, sizeof(float));
	// processor has precalculate correct value for us, but we do not setNormalized here
	// it can be just initial value and there will be more
	return kResultTrue;
      }
    }
  }
  return kResultFalse;
}


tresult PLUGIN_API Controller::setComponentState(IBStream* state){
  if(!state)
    return kResultFalse;

  //printf("Controller: setComponentState\n");

  IBStreamer streamer(state, kLittleEndian);

  int32 bypassState;
  if(streamer.readInt32(bypassState) == false)
    return kResultFalse;
  setParamNormalized(kBypassId, bypassState ? 1 : 0);

  char *soundFontFileName;
  if(soundFontFileName = streamer.readStr8()){
    delete soundFontFileName;
  }
  setParamNormalized(kRootPrgId, mCurrentProgram);
  // BAD SDK: it is goot time now, we used messege to transfer it
  //  It is unclear will host call GetState or SetState for processor in case of this one
  //  REAPER called GetState first (so "empty"), but then it can call SetState and setComponentState
  //  with the same (externally saved) state. So processor has no chance ot fix the state itself, effectively
  //  we can not use saved file name for that reason.

  return kResultOk;
}

tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 busIndex, int16 channel, Vst::CtrlNumber midiControllerNumber, Vst::ParamID& id/*out*/){
  if((midiControllerNumber >= Vst::kCountCtrlNumber) || !szCCName[midiControllerNumber][0])
    return kResultFalse;
  if(busIndex == 0){
    id = 1024 + channel*1024 + midiControllerNumber;
    // printf("ID = %d %d -> %d\n", channel, midiControllerNumber, id);
    return kResultOk;
  }
  // printf("Asked %d %d %d\n", busIndex, channel, midiControllerNumber);
  return kResultFalse;
}

tresult PLUGIN_API Controller::getUnitByBus(Vst::MediaType type, Vst::BusDirection dir, int32 busIndex, int32 channel, Vst::UnitID& unitId /*out*/){
  if(type == Vst::kEvent && dir == Vst::kInput && busIndex == 0){
    if((channel >= 0) && (channel < 16)){
      unitId = channel + 1;
      // printf("OK: %d %d %d %d\n", type, dir, busIndex, channel);
      return kResultTrue;
    }
  } else {
    // printf("Hmm: %d %d %d %d\n", type, dir, busIndex, channel);
  }
  return kResultFalse;
}

tresult PLUGIN_API Controller::setParamNormalized (Vst::ParamID tag, Vst::ParamValue value){
  tresult result = EditControllerEx1::setParamNormalized(tag, value);
  if(result == kResultOk){
    if(tag == kRootPrgId){
      //printf("Controller: SoundFont set to %f\n", value);
    }
  }
  return result;
}


} // FluidSynthVST

// Factory
BEGIN_FACTORY_DEF(stringCompanyName, stringCompanyWeb, stringCompanyEmail)
  DEF_CLASS2 (INLINE_UID_FROM_FUID(FluidSynthVST::ProcessorUID),
				PClassInfo::kManyInstances,
				kVstAudioEffectClass,
				stringPluginName,
				Vst::kDistributable,
				stringSubCategory,
				FULL_VERSION_STR,
				kVstVersionString,
				FluidSynthVST::Processor::createInstance)
  DEF_CLASS2 (INLINE_UID_FROM_FUID(FluidSynthVST::ControllerUID),
				PClassInfo::kManyInstances,
				kVstComponentControllerClass,
				stringPluginName "Controller",
				0,
				"",
				FULL_VERSION_STR,
				kVstVersionString,
				FluidSynthVST::Controller::createInstance)
END_FACTORY


extern void *moduleHandle;


#ifdef WIN32

#include <windows.h>
#include <glib.h>

extern "C" {
void g_thread_win32_process_detach (void);
void g_thread_win32_thread_detach (void);
void g_thread_win32_init (void);
void g_console_win32_init (void);
void g_clock_win32_init (void);
void g_crash_handler_win32_init (void);
void g_crash_handler_win32_deinit (void);
extern HMODULE glib_dll;

void glib_init(void);
};

// AZ: assume glib is build with Windows native threads
#define THREADS_WIN32 1

void glib_DllMain(void *moduleHandle, DWORD fdwReason, LPVOID lpvReserved){

  switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
      glib_dll = (HMODULE)moduleHandle;
      g_crash_handler_win32_init ();
      g_clock_win32_init ();
#ifdef THREADS_WIN32
      g_thread_win32_init ();
#endif
      glib_init ();
      /* must go after glib_init */
      g_console_win32_init ();

      // from gobject DllMain
      // gobject_init();

      break;

    case DLL_THREAD_DETACH:
#ifdef THREADS_WIN32
      g_thread_win32_thread_detach ();
#endif
      break;

    case DLL_PROCESS_DETACH:
#ifdef THREADS_WIN32
      if (lpvReserved == NULL)
        g_thread_win32_process_detach ();
#endif
      g_crash_handler_win32_deinit ();
      break;

    default:
      /* do nothing */
      ;
    }

}

void FluidSynthVST::GetPath(char *szPath /* Out */, int32 size){
  WCHAR wszPath[MAX_PATH];
  if(GetModuleFileName((HMODULE)moduleHandle, wszPath, MAX_PATH) > 0){
    wszPath[MAX_PATH - 1] = 0;
    if(WideCharToMultiByte(CP_UTF8, 0, wszPath, -1, szPath, size, NULL, NULL) > 0){
      szPath[size - 1] = 0;
      char *slash = strrchr(szPath, '\\');
      if(slash)
	*slash = 0;
      return;
    }
  }
  *szPath = 0;
}

void FluidSynthVST::PathAppend(char *szPath /* Out */, int32 size, const char *szName){
  if(size <= (strlen(szPath) + strlen(szName) + 1))
    return;
  strcat(szPath, "\\");
  strcat(szPath, szName);
}

void FluidSynthVST::Processor::scanSoundFonts(){
  WCHAR szName[MAX_PATH];
  if(GetModuleFileNameW((HMODULE)moduleHandle, szName, MAX_PATH) <= 0)
    return;
  szName[MAX_PATH - 1] = 0;
  WCHAR *slash = wcsrchr(szName, L'\\');
  if(slash)
    *slash = 0;
  wcscat(szName, L"\\*.sf?");

  HANDLE hFind;
  WIN32_FIND_DATA FindData;
  if((hFind = FindFirstFileW(szName, &FindData)) != INVALID_HANDLE_VALUE){
    do {
      slash = wcsrchr(FindData.cFileName, L'\\');
      if(slash)
	++slash;
      else
	slash = FindData.cFileName;
      String s(slash);
      s.toMultiByte(kCP_Utf8);
      const char *cstr = s.text8();
      // used pattern is translated using DOS wildcards, f.e. it will match name.sfpack
      if((strlen(cstr) > 3) && !memcmp(cstr + strlen(cstr) - 4, ".sf", 3))
	mSoundFontFiles.push_back( s );
    } while(FindNextFileW(hFind, &FindData));
    FindClose(hFind);
    std::sort(mSoundFontFiles.begin(), mSoundFontFiles.end());
  }
  if(mSoundFontFiles.size() == 0)
    mSoundFontFiles.push_back( "default.sf2" );
}

static DWORD WINAPI Processor_LoadingThread(void *par){
  auto processor = static_cast<FluidSynthVST::Processor *>(par);
  processor->syncedLoadSoundFont();
  return NULL;
}

/*
 * Returns true in case the synth can be used.
 * Initiate font loading, synced or asynced as specified in case that was requested.
 * Also finish loading in specified mode
 *
 * It is not thread safe, but it can be called from
 * destructor, setProcessing or process only. Host should never
 * parallelize any of them.
 */
bool FluidSynthVST::Processor::checkSoundFont(bool synced){
  //PerfMeter pm("Check", 8000);
  if(mLoadingThread){
    if(mLoadingComplete || synced){
      CloseHandle(mLoadingThread);
      mLoadingThread = 0;
      //printf("Processor: loading font complete\n");
    } else
      return false;
  }
  if(!mChangeSoundFont)
    return true;
  //printf("Processor: changing sound font %s\n", synced ? "synced" : "asynced");
  mLoadedFile = mSoundFontFile;
  mChangeSoundFont = false;
  if(!synced){
    mLoadingComplete = false;
    if((mLoadingThread = CreateThread(NULL, 0, Processor_LoadingThread, this, 0, NULL)))
      return false;
    printf("Could not create loading thread, continue in synced mode\n");
  }
  syncedLoadSoundFont();
  return true;
}


#else /* Linux */
#define glib_DllMain(x,y,z)

#include <dlfcn.h>

void FluidSynthVST::GetPath(char *szPath /* Out */, int32 size){
  Dl_info info;
  if(!dladdr(&moduleHandle, &info) || (strlen(info.dli_fname) > (size - 1))){
    *szPath = 0;
  } else {
    strcpy(szPath, info.dli_fname);
    char *slash = strrchr(szPath, '/');
    if(slash)
      *slash = 0;
  }
}

void FluidSynthVST::PathAppend(char *szPath /* Out */, int32 size, const char *szName){
  if(size <= (strlen(szPath) + strlen(szName) + 1))
    return;
  strcat(szPath, "/");
  strcat(szPath, szName);
}

#include <sys/types.h>
#include <dirent.h>

void FluidSynthVST::Processor::scanSoundFonts(){
  char szDirName[FILENAME_MAX];
  GetPath(szDirName, FILENAME_MAX);
  DIR *dir = opendir(szDirName);
  if(dir){
    struct dirent *de;
    while((de = readdir(dir))){
      if((strlen(de->d_name) > 4) && !memcmp(de->d_name + strlen(de->d_name) - 4, ".sf", 3)){
	mSoundFontFiles.push_back( de->d_name ); // will be in file system encoding, hope it is UTF8
      }
    }
    closedir(dir);
    std::sort(mSoundFontFiles.begin(), mSoundFontFiles.end());
  }
  if(mSoundFontFiles.size() == 0)
    mSoundFontFiles.push_back( "default.sf2" );
}

#include <pthread.h>

static void *Processor_LoadingThread(void *par){
  auto processor = static_cast<FluidSynthVST::Processor *>(par);
  processor->syncedLoadSoundFont();
  return NULL;
}

/*
 * Returns true in case the synth can be used.
 * Initiate font loading, synced or asynced as specified in case that was requested.
 * Also finish loading in specified mode
 *
 * It is not thread safe, but it can be called from
 * destructor, setProcessing or process only. Host should never
 * parallelize any of them.
 */
bool FluidSynthVST::Processor::checkSoundFont(bool synced){
  //PerfMeter pm("Check", 8000);
  if(mLoadingThread){
    if(mLoadingComplete || synced){
      pthread_join(mLoadingThread, NULL);
      mLoadingThread = 0;
      //printf("Processor: loading font complete\n");
    } else
      return false;
  }
  if(!mChangeSoundFont)
    return true;
  //printf("Processor: changing sound font %s\n", synced ? "synced" : "asynced");
  mLoadedFile = mSoundFontFile;
  mChangeSoundFont = false;
  if(!synced){
    mLoadingComplete = false;
    if(!pthread_create(&mLoadingThread, NULL, Processor_LoadingThread, this)){
      return false;
    }
    printf("Could not create loading thread, continue in synced mode\n");
  }
  syncedLoadSoundFont();
  return true;
}

#endif


// Module
bool InitModule(){
  glib_DllMain(moduleHandle, DLL_PROCESS_ATTACH, NULL);
  return true;
}

bool DeinitModule(){
  glib_DllMain(moduleHandle, DLL_PROCESS_DETACH, NULL);
  return true;
}


