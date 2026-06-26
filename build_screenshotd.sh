#!/bin/bash
qmake6 hyprtile-screenshotd.pro
make -j10
strip --strip-all hypertile-screenshotd
upx --ultra-brute hypertile-screenshotd
