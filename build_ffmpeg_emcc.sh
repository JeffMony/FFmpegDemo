FFMPEG_SOURCE_DIR=/Users/jefflee/source/FFmpeg

CURRENT_DIR=$(pwd)

cd ${FFMPEG_SOURCE_DIR}

make clean

emconfigure ./configure \
--cc="emcc" \
--cxx="em++" \
--ar="emar" \
--ranlib=emranlib \
--prefix=${CURRENT_DIR}/ffmpeg_dist \
--enable-cross-compile \
--target-os=none \
--arch=arm64 \
--cpu=generic \
--enable-gpl \
--enable-version3 \
--disable-avdevice \
--disable-sdl2 \
--disable-iconv \
--disable-asm \
--disable-x86asm \
--disable-inline-asm \
--disable-programs \
--disable-doc \
--disable-ffplay \
--disable-ffprobe \
--disable-ffmpeg


make

make install


cd -