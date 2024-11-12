import fs from 'fs';
import Zstd from './wasm/zstd.js';

const zstd = await Zstd();

const decompress = zstd.cwrap('ZSTD_decompress', 'number', ['number', 'number', 'number', 'number']);
const getFrameContentSize = zstd.cwrap('ZSTD_getFrameContentSize', 'number', ['number', 'number']);

export default function dumpDecompressedImages() {

    const zstdCompressedFrameOffsets = [];

    const assetBundle3Buffer = fs.readFileSync('./cache/assetBundle3');

    for (let i = 0; i < assetBundle3Buffer.length - 4; i++) {
        const magic = assetBundle3Buffer.readUInt32BE(i);
        if (magic === 0x28B52FFD) {
            zstdCompressedFrameOffsets.push(i);
        }
    }

    console.log(zstdCompressedFrameOffsets.length);
    for (let i = 0; i < zstdCompressedFrameOffsets.length; i++) {
        const offset = zstdCompressedFrameOffsets[i];

        if (i % 500 === 0) {
            console.log(i);
        }

        let nextOffset = i === zstdCompressedFrameOffsets.length - 1 ? assetBundle3Buffer.length : zstdCompressedFrameOffsets[i + 1];

        let {statusCode, output} = read(assetBundle3Buffer.subarray(offset, nextOffset));

        if (statusCode <= 0) {
            output = read(assetBundle3Buffer.subarray(offset, nextOffset - 39)).output;
        }

        const outFolder = './out/decompressed';
        fs.mkdirSync(outFolder, {recursive: true});

        fs.writeFileSync(`${outFolder}/${i}.bin`, Buffer.from(output));
    }
};

function read(input) {
    const inputSize = input.length;
    const inputPtr = zstd._malloc(inputSize);

    zstd.HEAPU8.set(input, inputPtr);
    const decompressedSize = getFrameContentSize(inputPtr, inputSize);
    const output = new Uint8Array(decompressedSize);
    const outputPtr = zstd._malloc(decompressedSize);

    const statusCode = decompress(outputPtr, decompressedSize, inputPtr, inputSize);

    output.set(zstd.HEAPU8.subarray(outputPtr, outputPtr + decompressedSize));

    zstd._free(inputPtr);
    zstd._free(outputPtr);

    return {statusCode, output};
}