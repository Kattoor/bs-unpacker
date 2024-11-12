import fs from 'fs';
import {Image} from 'image-js';
import Bcdec from './wasm/bcdec.js';

const bcdec = await Bcdec();

const bcdec_bc1 = bcdec.cwrap('bcdec_bc1', null, ['number', 'number', 'number', 'number', 'number']);
const bcdec_bc3 = bcdec.cwrap('bcdec_bc3', null, ['number', 'number', 'number', 'number', 'number']);
const bcdec_bc4 = bcdec.cwrap('bcdec_bc4', null, ['number', 'number', 'number', 'number', 'number']);
const bcdec_bc5 = bcdec.cwrap('bcdec_bc5', null, ['number', 'number', 'number', 'number', 'number']);
const bcdec_r8 = bcdec.cwrap('bcdec_r8', null, ['number', 'number', 'number', 'number', 'number']);

function getAverageBlockColor(data) {
    function combine(high, low) {
        return Math.floor(low + (high - low) / 3);
    }

    const r1 = (data[1] & 0b11111000);
    const g1 = ((data[1] & 0b00000111) << 5) + ((data[0] & 0b11100000) >> 3);
    const b1 = (data[0] & 0b00011111) << 3;
    const r2 = (data[3] & 0b11111000);
    const g2 = ((data[3] & 0b00000111) << 5) + ((data[2] & 0b11100000) >> 3);
    const b2 = (data[2] & 0b00011111) << 3;
    const r = combine(r1, r2);
    const g = combine(g1, g2);
    const b = combine(b1, b2);

    return [r, g, b];
}

function guessColorUpper(data) {
    const blocks = Math.floor(data.length / 4);
    let goodBlocks = 0;
    let prevColor = [0, 0, 0];
    let greenCount = 0;
    for (let pixel = 0; pixel < data.length; pixel += 4) {
        let color = getAverageBlockColor(data.slice(pixel, pixel + 4));
        let dist = (color[0] - prevColor[0]) ** 2 + (color[1] - prevColor[1]) ** 2 + (color[2] - prevColor[2]) ** 2;
        if (JSON.stringify(color) === JSON.stringify([0, 160, 0])) {
            greenCount += 1;
            dist = 1000000
        }
        if (dist < 1000) {
            goodBlocks += 1;
        }
        prevColor = color;
    }
    if (greenCount > 10) {
        return -1;
    }
    return goodBlocks / blocks;
}

function guessColorLower(data) {
    return 1 - guessColorUpper(data);
}

function guessSmoothUpper(data) {
    const blocks = Math.floor(data.length / 2);
    let greenBlocks = 0;
    for (let pixel = 0; pixel < data.length; pixel += 2) {
        if (data[pixel] === 0 && data[pixel + 1] === 5) {
            greenBlocks += 1;
        }
    }
    return greenBlocks / blocks;
}

function guessSmoothLower(data) {
    const blockThirds = data.length;
    let solidBlockThirds = 0;
    for (let pixel = 0; pixel < data.length; pixel++) {
        if (data[pixel] === 0 || data[pixel] === 0xff) {
            solidBlockThirds += 1;
        }
    }
    return solidBlockThirds / blockThirds;
}

function guessFiletype(data) {
    const size = data.length;
    const possibilities = {
        "BC1": true,
        "BC2": true,
        "BC3": true,
        "BC4": true,
        "BC5": true,
        "R8": true,
    };

    const score = {
        "BC1": 0,
        "BC2": 0,
        "BC3": 0,
        "BC4": 0,
        "BC5": 0,
        "R8": 0,
    };

    if (size % 16 !== 0) {
        possibilities["BC2"] = false;
        possibilities["BC3"] = false;
        possibilities["BC5"] = false;
    }
    if (size % 8 !== 0) {
        possibilities["BC1"] = false;
        possibilities["BC4"] = false;
    }

    if (possibilities["BC1"]) {
        score["BC1"] += guessColorUpper(data.slice(0, Math.floor(size / 2)));
        score["BC1"] += guessColorLower(data.slice(Math.floor(size / 2)));
    }
    if (possibilities["BC2"]) {
        // TODO: Handle BC2
    }
    if (possibilities["BC3"]) {
        const subSize = Math.floor(size / 8);
        score["BC3"] += guessSmoothUpper(data.slice(0, subSize));
        score["BC3"] += guessColorUpper(data.slice(subSize, subSize * 2));
        score["BC3"] += guessSmoothLower(data.slice(subSize * 3, subSize * 6));
        score["BC3"] += guessColorLower(data.slice(subSize * 6, subSize * 8));
        score["BC3"] /= 2;
    }
    if (possibilities["BC4"]) {
        score["BC4"] += guessSmoothUpper(data.slice(0, Math.floor(size / 2)));
        score["BC4"] += guessSmoothLower(data.slice(Math.floor(size / 2)));
    }
    if (possibilities["BC5"]) {
        // TODO: Handle BC5
    }

    Object.keys(possibilities).forEach((key) => {
        if (!possibilities[key]) {
            delete score[key];
        }
    });

    return Object.keys(score).reduce((a, b) => score[a] > score[b] ? a : b);
}

class Format {
    constructor(func, blockSize, pixelsPerBlock) {
        this.func = func;
        this.blockSize = blockSize;
        this.pixelsPerBlock = pixelsPerBlock;
        this.blockDimensions = Math.floor(Math.sqrt(pixelsPerBlock));
    }
}

const formats = {
    BC1: new Format(bcdec_bc1, 8, 16),
    BC3: new Format(bcdec_bc3, 16, 16),
    BC4: new Format(bcdec_bc4, 8, 16),
    BC5: new Format(bcdec_bc5, 16, 16),
    R8: new Format(bcdec_r8, 1, 1)
};

class DxtProperties {
    static RGBA_SIZE = 4;

    static calculateFactors(size) {
        let height = Math.floor(Math.sqrt(size));
        let width = size / height;
        while (!Number.isInteger(width)) {
            height -= 1;
            width = size / height;
        }
        return [width, height];
    }

    constructor(data, format) {
        this.format = format;
        this.dataSize = data.length;
        this.blockCount = Math.floor(data.length / this.format.blockSize);
        this.pixelCount = this.blockCount * this.format.pixelsPerBlock;
        this.bufferSize = this.pixelCount * DxtProperties.RGBA_SIZE;
        this.guessDimensions();
    }

    guessDimensions() {
        const factors = DxtProperties.calculateFactors(this.blockCount);
        this.blocksWidth = factors[0];
        this.blocksHeight = factors[1];
        this.width = this.blocksWidth * this.format.blockDimensions;
        this.height = this.blocksHeight * this.format.blockDimensions;
    }
}

export default async function convertImages() {
    const amountOfFiles = fs.readdirSync('./out/decompressed').length;
    for (let i = 0; i < amountOfFiles; i++) {
        if (!fs.existsSync(`./out/decompressed/${i}.bin`)) {
            console.log('Missing', i);
            continue;
        }

        if (i % 500 === 0) {
            console.log(i);
        }

        const rawData = fs.readFileSync(`./out/decompressed/${i}.bin`);
        const format = formats[guessFiletype(rawData)];

        const imgProperties = new DxtProperties(rawData, format);
        const compressedFrame = new Uint8Array(rawData);

        const compressedFramePtr = bcdec._malloc(compressedFrame.length);
        const decompressedFramePtr = bcdec._malloc(imgProperties.bufferSize);
        bcdec.HEAPU8.set(compressedFrame, compressedFramePtr);

        try {
            format.func(compressedFramePtr, decompressedFramePtr, imgProperties.blocksWidth, imgProperties.blocksHeight, imgProperties.dataSize);
        } catch (e) {

        }

        const decompressedFrame = new Uint8Array(bcdec.HEAPU8.subarray(decompressedFramePtr, decompressedFramePtr + imgProperties.bufferSize));

        bcdec._free(compressedFramePtr);
        bcdec._free(decompressedFramePtr);

        const colorData = Buffer.from(decompressedFrame);
        const image = new Image({
            width: imgProperties.width, height: imgProperties.height, data: colorData, kind: 'RGBA'
        });

        const outFolder = './out/images';
        fs.mkdirSync(outFolder, {recursive: true});
        await image.save(`${outFolder}/${i}.png`);
    }

};
