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
#pragma once

#include "pluginterfaces/base/fplatform.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstmessage.h"

#include "fluidsynth.h"

#ifdef WIN32
#include <windows.h>
#else /* Linux */
#include <pthread.h>
#endif /* Platform */

#define MAJOR_VERSION_STR "0"
#define MAJOR_VERSION_INT 0

#define SUB_VERSION_STR "0"
#define SUB_VERSION_INT 0

#define RELEASE_NUMBER_STR "0"
#define RELEASE_NUMBER_INT 0

#define BUILD_NUMBER_STR "2"
#define BUILD_NUMBER_INT 2

#define FULL_VERSION_STR MAJOR_VERSION_STR "." SUB_VERSION_STR "." RELEASE_NUMBER_STR "." BUILD_NUMBER_STR

#define VERSION_STR MAJOR_VERSION_STR "." SUB_VERSION_STR "." RELEASE_NUMBER_STR

#define stringPluginName	"FluidSynth"
#define stringSubCategory       "Instrument|Sampler"

#define stringOriginalFilename	"fluidsynthvst.vst3"
#define stringFileDescription	stringPluginName

#define stringCompanyName	"AZ"
#define stringCompanyWeb	"http://www.azslow.com"
#define stringCompanyEmail	"mailto:admin@azslow.com"

#define stringLegalCopyright	"© 2019 AZ"
#define stringLegalTrademarks	"VST is a trademark of Steinberg Media Technologies GmbH"

namespace FluidSynthVST {
using namespace Steinberg;

static const FUID ProcessorUID(0xe42276cd, 0x074c42e5, 0xb68a740f, 0x8565fc6f);
static const FUID ControllerUID(0x43d1bc05, 0xf0f847a0, 0x85153e69, 0xe59c7455);

enum  FluidSynthVSTParams : Vst::ParamID {
    kBypassId = 0,

    // Vst::ProgramListID, but they are used as program change ParamID...
    kRootPrgId,
    kChPrgId,
    kLastChPrgId = kChPrgId + 15,
};


class Controller : public Vst::EditControllerEx1, public Vst::IMidiMapping {
  public:
    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
    tresult PLUGIN_API setComponentState(IBStream* state) SMTG_OVERRIDE;
    tresult PLUGIN_API setParamNormalized (Vst::ParamID tag, Vst::ParamValue value) SMTG_OVERRIDE;

    tresult PLUGIN_API getMidiControllerAssignment (int32 busIndex, int16 channel, Vst::CtrlNumber midiControllerNumber, Vst::ParamID& id/*out*/) SMTG_OVERRIDE;

    tresult PLUGIN_API getUnitByBus (Vst::MediaType type, Vst::BusDirection dir, int32 busIndex, int32 channel, Vst::UnitID& unitId /*out*/) SMTG_OVERRIDE;

    tresult PLUGIN_API notify (Vst::IMessage* message) SMTG_OVERRIDE;

    static FUnknown* createInstance (void*){
      return (IEditController*)new Controller();
    }

    //static FUID cid;

    OBJ_METHODS (Controller, Vst::EditControllerEx1)
    DEFINE_INTERFACES
      DEF_INTERFACE(Vst::IMidiMapping)
    END_DEFINE_INTERFACES(Vst::EditControllerEx1)
    REFCOUNT_METHODS(Vst::EditControllerEx1)

  protected:
};

class Processor : public Vst::AudioEffect {
  public:
    Processor();
    ~Processor();
    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
    tresult PLUGIN_API setBusArrangements(Vst::SpeakerArrangement* inputs, int32 numIns,
					  Vst::SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE;

    tresult PLUGIN_API canProcessSampleSize (int32 symbolicSampleSize) SMTG_OVERRIDE;
    tresult PLUGIN_API setupProcessing (Vst::ProcessSetup& setup) SMTG_OVERRIDE;
    tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE;
    tresult PLUGIN_API setProcessing (TBool state) SMTG_OVERRIDE;
    tresult PLUGIN_API process(Vst::ProcessData& data) SMTG_OVERRIDE;

    tresult PLUGIN_API setState (IBStream* state) SMTG_OVERRIDE;
    tresult PLUGIN_API getState (IBStream* state) SMTG_OVERRIDE;

    tresult PLUGIN_API connect (IConnectionPoint* other) SMTG_OVERRIDE;

    static FUnknown* createInstance(void*){
      return (Vst::IAudioProcessor*)new Processor ();
    }

    void    syncedLoadSoundFont(); // public for thread function

  protected:
    bool mBypass = false;

    fluid_settings_t* mSynthSettings;
    fluid_synth_t* mSynth;
    int32          mSoundFoundID; // -1 when failed to load

    using StringVector = std::vector<String>;
    StringVector mSoundFontFiles; // in UTF-8
    String       mSoundFontFile;
    bool         mChangeSoundFont;

    float  *mAudioBufs[2];
    int32   mAudioBufsSize;


    void  writeAudio(Vst::ProcessData& data, int32 start_sample, int32 end_samle);
    int32 nextOffset(Vst::ProcessData& data, int32 curSample);
    void  playParChanges(Vst::ProcessData& data, int32 curSample, int32 endSample);
    void  playEvents(Vst::ProcessData& data, int32 curSample, int32 endSample);

    void  scanSoundFonts();
    bool  checkSoundFont(bool synced);
    float getCurrentSoundFontNormalized();
    void  sendCurrentProgram();

  private:
#ifdef WIN32
    HANDLE     mLoadingThread;
#else /* Linux */
    pthread_t  mLoadingThread;
#endif /* platform */
    String       mLoadingFile;
    bool         mLoadingComplete;
};


void GetPath(char *szPath /* Out */, int32 size);
void PathAppend(char *szPath /* Out */, int32 size, const char *szName);

}

