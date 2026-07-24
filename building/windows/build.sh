#!/bin/bash

rm -rf /c/Users/vboxuser/Documents/releasebuild/src/
cp -r /z/RELEASE4/src/ /c/Users/vboxuser/Documents/releasebuild/

printf "f\ng\nQ\n" | lua5.1 build.lua

mingw32-make

cp /c/Users/vboxuser/Documents/releasebuild/tachyonfire.exe /z/RELEASE4/tachyonfire.exe

wait