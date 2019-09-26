cmake_minimum_required (VERSION 3.9)

# That will only work for Makefile and Ninja (single type in the tree build system)
# For VS/XCode, CMAKE_CONFIGURATION_TYPES can be set to limit variants
#set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)


project(FluidSynthVST)

set(CMAKE_POSITION_INDEPENDENT_CODE on)

include(VST3_SDK.cmake)

if ( WIN32 )
include_directories(fluidsynth/include fluidsynth/build/include glib/include/glib-2.0 glib/lib/glib-2.0/include)
link_directories(fluidsynth/build/src glib/lib)
else ( WIN32 )
include_directories(fluidsynth/include fluidsynth/build/include)
link_directories(fluidsynth/build/src)
endif ( WIN32 )


set(plug_sources
    include/fluidsynthvst.h
    source/fluidsynthvst.cpp
)

set(target fluidsynthvst)

smtg_add_vst3plugin(${target} ${plug_sources})

if ( WIN32 )
target_link_libraries(${target} PRIVATE sdk libfluidsynth glib-2.0 gthread-2.0 intl ws2_32)
else ( WIN32 )
target_link_libraries(${target} PRIVATE sdk fluidsynth)
endif ( WIN32 )

smtg_dump_plugin_package_variables(${target})
cmake_print_variables(CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES)
