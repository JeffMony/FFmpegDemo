CJSON_SOURCE_DIR=$(pwd)/cJSON

CURRENT_DIR=$(pwd)

cd ${CJSON_SOURCE_DIR}

emcmake cmake . -DENABLE_CJSON_UTILS=On \
-DENABLE_CJSON_TEST=Off \
-DCMAKE_INSTALL_PREFIX=${CURRENT_DIR}/cjson_wasm_dist \
-DBUILD_SHARED_LIBS=Off

emmake make

emmake make install

cd -