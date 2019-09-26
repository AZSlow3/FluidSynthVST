# FluidSynth wrapped into VST3i plug-in
2019 AZ (www.azslow.com)

Lightweight SoundFont 2 (SF2) based multi-instrument synth, capable to
play general MIDI files within a DAW.

## Why I have made it
For long time I was looking for a simple GM VST, which is able to
play "normal" multi-instruments MIDI files, can be used for sketching
with a MIDI keyboard, works natively under Windows and Linux, does not
has tricky authentication procedure or other use restrictions and preferably
open source.

It can be I have reinvented the wheel, but after I have not found any
such plug-in, I have decided to compose one.

## License

The soruce code for the wrapper is destributed under the terms of
GNU General Public License (GPL) v3.

FluidSynth and glib are dustrubuted under GNU Lesser General Public
License (LGPL)

Steinberg VST3 SDK is used within this plug-in under GPL v3.

## Credits

All credits should go to FluidSynth developers: https://github.com/FluidSynth/fluidsynth

## How to use
- Build or download FluidSynthVST.vst3 for your platform.
   Archives for Linux/Ubuntu 18.04 64bit and Windows 64bit can be found in the
   bin subdirectory
- download some SF2 with sounds you need (see FluidSynth documentation for some or just
   search the Internet for GM SoundFont 2. Put it as "default.sf2" into the same directory
- point your DAW to use the directory for VST3

## Known limitations
- there is no GUI
- it is not possible to change used font on the fly
- current instrument settings are not saved (MIDI source normally have them)
- stereo output only
