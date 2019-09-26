#!/bin/bash
cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=on -DBUILD_SHARED_LIBS=off -DCMAKE_C_FLAGS=-fPIC -Denable-libsndfile=off -Denable-jack=off -Denable-dbus=off -Denable-alsa=off -Denable-oss=off -Denable-ladspa=off -Denable-network=off




