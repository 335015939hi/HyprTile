#!/bin/bash

rm sendtoggle

g++ sendtoggle.cpp -o sendtoggle \
   -Os -s -fvisibility=hidden -fvisibility-inlines-hidden \
   -ffunction-sections -fdata-sections -Wl,--gc-sections \
   -fno-exceptions -fno-rtti


strip --strip-all sendtoggle

upx --ultra-brute sendtoggle
