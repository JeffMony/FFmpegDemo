FFMPEG_DIST=$(pwd)/ffmpeg_dist


emcc -O3 -s WASM=1 \
-I ${FFMPEG_DIST}/include \
-L ${FFMPEG_DIST}/lib \
-l avcodec -l avformat -l swresample -l swscale -l avutil \
-o media_extractor_wasm.js media_extractor.c