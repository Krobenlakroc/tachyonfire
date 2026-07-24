FROM debian:buster

RUN sed -i 's/deb.debian.org/archive.debian.org/g' /etc/apt/sources.list
RUN sed -i 's/security.debian.org/archive.debian.org/g' /etc/apt/sources.list

RUN apt update
RUN apt install -y gcc make 
RUN apt install -y lua5.1
RUN apt install -y build-essential libgl1-mesa-dev
RUN apt install -y wget unzip cmake libasound2 libasound2-dev pulseaudio libpulse-dev

RUN apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config \
    libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev \
    libgl1-mesa-dev libglu1-mesa-dev \
    libasound2-dev libpulse-dev \
    libudev-dev libdbus-1-dev

#OPENAL
RUN mkdir /openal/
WORKDIR /openal/
RUN wget https://github.com/kcat/openal-soft/archive/refs/tags/1.22.2.zip
RUN unzip 1.22.2.zip
WORKDIR /openal/openal-soft-1.22.2/build
RUN cmake ..
RUN cmake --build .

#FREETYPE
RUN mkdir /freetype/
WORKDIR /freetype/
RUN wget https://download.savannah.gnu.org/releases/freetype/freetype-2.14.0.tar.gz
RUN tar -xvzf freetype-2.14.0.tar.gz
WORKDIR /freetype/freetype-2.14.0/
RUN ./configure --with-zlib=no --with-bzip2=no --with-png=no --with-harfbuzz=no --with-brotli=no --with-librsvg=no
RUN make

#SDL2
RUN mkdir /SDL2/
WORKDIR /SDL2/
RUN wget https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.32.4.zip
RUN unzip release-2.32.4.zip
WORKDIR /SDL2/SDL-release-2.32.4/
RUN ./configure
RUN make

#GCC 15
RUN mkdir /gccnew/
WORKDIR /gccnew/
RUN wget https://ftp.gnu.org/gnu/gcc/gcc-15.2.0/gcc-15.2.0.tar.xz
RUN apt install -y xz-utils bzip2 g++
RUN tar xf gcc-15.2.0.tar.xz
WORKDIR /gccnew/gcc-15.2.0/
RUN ./contrib/download_prerequisites
RUN mkdir build
WORKDIR /gccnew/gcc-15.2.0/build
RUN ../configure --prefix=/opt/gcc-15 --enable-languages=c --disable-multilib --disable-libsanitizer
RUN make -j$(nproc)
RUN make install


#Tachyon build environment
RUN mkdir /tachyon/
WORKDIR /tachyon/
#ftinclude
#include
#src
#build.lua
#bin/lib64
#bin/o/
RUN mkdir ./ftinclude/
RUN mkdir ./include/
RUN mkdir ./bin/
RUN mkdir ./bin/o/
RUN mkdir ./bin/o/fn_game/
RUN mkdir ./bin/o/fn_math/
RUN mkdir ./bin/o/fn_engine/
RUN mkdir ./bin/lib64/

RUN cp /SDL2/SDL-release-2.32.4/build/.libs/libSDL2-2.0.so.0 ./bin/lib64/libSDL2-2.0.so.0
RUN cp /SDL2/SDL-release-2.32.4/build/.libs/libSDL2-2.0.so.0.3200.4 ./bin/lib64/libSDL2-2.0.so.0.3200.4
RUN cp /openal/openal-soft-1.22.2/build/libopenal.so.1.22.2 ./bin/lib64/libopenal.so.1.22.2 
RUN cp /openal/openal-soft-1.22.2/build/libopenal.so.1 ./bin/lib64/libopenal.so.1
RUN cp /openal/openal-soft-1.22.2/build/libopenal.so ./bin/lib64/libopenal.so

RUN cp /freetype/freetype-2.14.0/objs/.libs/libfreetype.a ./libfreetype.a

RUN cp -r /freetype/freetype-2.14.0/include/dlg/. ./ftinclude/dlg
RUN cp /freetype/freetype-2.14.0/include/ft2build.h ./ftinclude/ft2build.h
RUN cp -r /freetype/freetype-2.14.0/include/freetype/. ./ftinclude/freetype

RUN mkdir ./include/SDL2/

RUN cp -r /SDL2/SDL-release-2.32.4/include/. ./include/SDL2/
RUN cp -r /openal/openal-soft-1.22.2/include/AL/. ./include/AL/

COPY ./build.lua /tachyon/build.lua
COPY ./src /tachyon/src
COPY ./include/glad /tachyon/include/glad
COPY ./include/KHR /tachyon/include/KHR

RUN printf "f\ng\nc\nQ\n" | lua build.lua
# RUN PATH=/opt/gcc-15/bin:$PATH make


