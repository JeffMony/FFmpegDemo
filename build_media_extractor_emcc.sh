FFMPEG_DIST_DIR=$(pwd)/ffmpeg_wasm_dist
CJSON_DIST_DIR=$(pwd)/json_wasm_dist
CJSON_SOURCE_DIR=$(pwd)/cJSON

emcc -O3 \
-I ${FFMPEG_DIST_DIR}/include \
-L ${FFMPEG_DIST_DIR}/lib -l avcodec -l avformat -l swresample -l avutil \
-I ${CJSON_DIST_DIR}/include \
-s EXPORTED_FUNCTIONS="['_malloc', '_free', 'UTF8ToString', '_allocate_array_size','_set_buffer_index', '_get_result_json', '_get_current_time', '_extract_video_data']" \
-s WASM=1 -s ASSERTIONS=0 -s TOTAL_MEMORY=167772160 -s ALLOW_MEMORY_GROWTH=1 \
-o media_extractor_emcc.js media_extractor_emcc.c ${CJSON_SOURCE_DIR}/cJSON.c ${CJSON_SOURCE_DIR}/cJSON_Utils.c 