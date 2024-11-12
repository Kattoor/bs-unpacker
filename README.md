# BS Unpacker


## Run
1. Add your assetBundle3 file to bs-unpacker/cache
2. `npm install`
3. `node main.mjs`
4. wait

## WASM
If you want to build the WASM files yourself (not necessary to run the project), you can use the following Emscripten commands:

`emcc "bs-unpacker\libs\zstd\zstd.c" -o "bs-unpacker\wasm\zstd.js" -s EXPORTED_FUNCTIONS="['_ZSTD_getFrameContentSize', '_ZSTD_decompress', '_malloc', '_free']" -s EXPORTED_RUNTIME_METHODS="['cwrap']" -s WASM=1 -s MODULARIZE=1 -s ENVIRONMENT=node -s EXPORT_ES6=1`

`emcc "bs-unpacker\libs\bcdec\bcdec.c" -o "bs-unpacker\wasm\bcdec.js" -s EXPORTED_FUNCTIONS="['_bcdec_bc1', '_bcdec_bc3', '_bcdec_bc4', '_bcdec_bc5', '_bcdec_r8', '_malloc', '_free']" -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap']" -s WASM=1 -s MODULARIZE=1 -s ENVIRONMENT=node -s EXPORT_ES6=1 -s ALLOW_MEMORY_GROWTH`
