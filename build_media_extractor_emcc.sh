FFMPEG_DIST_DIR=$(pwd)/ffmpeg_dist


emcc -O3 \
-I ${FFMPEG_DIST_DIR}/include \
-L ${FFMPEG_DIST_DIR}/lib -l avcodec -l avformat -l swresample -l avutil \
-s EXPORTED_FUNCTIONS="['_malloc', '_free', 'UTF8ToString', '_get_current_time', '_return_error', '_custom_log_callback', '_extract_video_info', '_main']" \
-s WASM=1 -s ASSERTIONS=0 -s TOTAL_MEMORY=167772160 -s ALLOW_MEMORY_GROWTH=1 \
-o media_extractor.js media_extractor.c