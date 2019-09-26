FluidSynthVST build instructions.
2019 AZ (www.azslow.com)

Intro:
--------------------------------------------------------------
I prefer static fluidsynth library, for portability.
On Linux, it is dynamically linked with system glib.
On Window, I build static version of glib first.
In case you prefer to link everything dynamic (default for FluidSynth and the only
available for glib without hacks), you will need to modify cmake files and
the plug-in source.


Build preparation:
--------------------------------------------------------------
  (in the directory with README file)

  download VST3 SDK (3.6+) and copy VST3_SDK folder there (alternatively add "-D VST3_SDK_ROOT=xxx" when you have it at other location)

  download FluidSynth (I have used 2.0.5) and copy the content into fluidsynth directory

  (Windows) downaload glib (I have 2.62.0) and put into FluidSynthVST directory (with original name, so glib-2.62.0)

--------------------------------------------------------------
Build on Linux (assuming system platform, in my case amd64):
  (glib/gtk development package and cmake should be installed on the system level)
  cd fluidsynth/build
  ./prepare.sh
  (check if there are any errors, check fluidsynth docs to solve)
  make
  cd ../..

  mkdir build
  cd build
  cmake -DCMAKE_BUILD_TYPE=Release ../
  make

---------------------------------------------------------------
Windows:
  Preparation:
  install Visual Studio (use MS documentation, only msbuild is used in the following, not IDE)
  cmake (installer)
  git
  Python (set "add to system path" during installation)
  meson (pip3 install meson)

  Note: if you have vcpkg installed, disable it (vcpkg integrate remove). It can clash with the following build procedure otherwise.

  Build static glib.
  ------------------
  For now, official glib 2.62.0 does not support static build on Linux. So build procedure is a bit hacky.

  Patch the source:
    1) comment all code which define DllMain unconditionally, in current version it is in
       glib/glib-init.c and gobject/gtype.c
       I simply surround it with "#ifdef DLL_EXPORT" (do not comment glib_dll global variable!), so the same source still can be used for normal build
    2) adopt glib_DllMain in fluidsynthvst.cpp to be exactly as DllMain in glib/glib-init.c.
       My source is for glib 2.62.0
       Note that fluidsynth does not use gobject, so comment its initialaization (I do not link with it).

  mkdir build
  meson .. --buildtype=release --prefix=<absolute path to FluidSynthVST>/glib --backend=vs -Ddefault_library=static -Dc_args="-DFFI_STATIC_BUILD -DG_INTL_STATIC_COMPILATION"

  msbuld glib.sln /p:Configuration=Release
   (That will take a while. Can be avoided by disabling build_tests in meson configuration files, but the quite some
    files should be touched during installation)
   (there will be quite some errors... but if libglib-2.0.a is generated, it should be fine)

  type nul > subproject\proxy-libintl\intl.lib
   (that will create dummy file for the next command)
  meson install

  cd ../../glib/lib
  del intl.*
  rename (move) libXXX.a into XXX.lib for XXX = (glib-2.0, intl, gthread)
    (meson build "libXXX.a" instead of "XXX.lib" on Windows, to not clash with .lib export files for DLLs)

  At this point you can delete glib-2.62.0 directory.

  Build static fluidsynth library
  --------------
  cd fluidsynth\build

  modify prepare.bat and put absolute path to build structure (my is E:\FluidSynthVST)

  prepare.bat
  msbuild /p:Configuration=[Release|Debug] FluidSynth.sln
    (build will have many linking errors, that is ok as soon as src\Release\libfluidsynth.lib is generated)


  Build FluidSynthVST
  ----------------

  mkdir build
  cd build
  cmake -A x64 ../
  msbuild /p:Configuration=Release FluidSynthVST.sln

  ( cmake -A [x64|x86] ../
    msbuild /p:Configuration=[Release|Debug] FluidSynthVST.sln
  )
