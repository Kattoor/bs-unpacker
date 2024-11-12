// https://github.com/iOrange/bcdec/blob/main/bcdec.h

#include <stdio.h>

#define BLOCK_WIDTH 4
#define BLOCK_HEIGHT 4
#define RGBA_SIZE 4
#define RGB565_SIZE 2

static void bcdec__color_block(const unsigned short* compressedColors, const unsigned int* compressedColorIndices, void* decompressedBlock, int destinationPitch, int onlyOpaqueMode) {
    unsigned short c0, c1;
    unsigned int refColors[4]; /* 0xAABBGGRR */
    unsigned char* dstColors;
    unsigned int colorIndices;
    int i, j, idx;
    unsigned int r0, g0, b0, r1, g1, b1, r, g, b;

    c0 = compressedColors[0];
    c1 = compressedColors[1];

    /* Unpack 565 ref colors */
    r0 = (c0 >> 11) & 0x1F;
    g0 = (c0 >> 5)  & 0x3F;
    b0 =  c0        & 0x1F;

    r1 = (c1 >> 11) & 0x1F;
    g1 = (c1 >> 5)  & 0x3F;
    b1 =  c1        & 0x1F;

    /* Expand 565 ref colors to 888 */
    r = (r0 * 527 + 23) >> 6;
    g = (g0 * 259 + 33) >> 6;
    b = (b0 * 527 + 23) >> 6;
    refColors[0] = 0xFF000000 | (b << 16) | (g << 8) | r;

    r = (r1 * 527 + 23) >> 6;
    g = (g1 * 259 + 33) >> 6;
    b = (b1 * 527 + 23) >> 6;
    refColors[1] = 0xFF000000 | (b << 16) | (g << 8) | r;

    if (c0 > c1 || onlyOpaqueMode) {    /* Standard BC1 mode (also BC3 color block uses ONLY this mode) */
        /* color_2 = 2/3*color_0 + 1/3*color_1
           color_3 = 1/3*color_0 + 2/3*color_1 */
        r = ((2 * r0 + r1) *  351 +   61) >>  7;
        g = ((2 * g0 + g1) * 2763 + 1039) >> 11;
        b = ((2 * b0 + b1) *  351 +   61) >>  7;
        refColors[2] = 0xFF000000 | (b << 16) | (g << 8) | r;

        r = ((r0 + r1 * 2) *  351 +   61) >>  7;
        g = ((g0 + g1 * 2) * 2763 + 1039) >> 11;
        b = ((b0 + b1 * 2) *  351 +   61) >>  7;
        refColors[3] = 0xFF000000 | (b << 16) | (g << 8) | r;
    } else {                            /* Quite rare BC1A mode */
        /* color_2 = 1/2*color_0 + 1/2*color_1;
           color_3 = 0;                         */
        r = ((r0 + r1) * 1053 +  125) >>  8;
        g = ((g0 + g1) * 4145 + 1019) >> 11;
        b = ((b0 + b1) * 1053 +  125) >>  8;
        refColors[2] = 0xFF000000 | (b << 16) | (g << 8) | r;

        refColors[3] = 0x00000000;
    }

    colorIndices = compressedColorIndices[0];

    /* Fill out the decompressed color block */
    dstColors = (unsigned char*)decompressedBlock;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            idx = colorIndices & 0x03;
            ((unsigned int*)dstColors)[j] = refColors[idx];
            colorIndices >>= 2;
        }

        dstColors += destinationPitch;
    }
}


static void bcdec__smooth_alpha_block(const unsigned char* compressedAlphas, const unsigned char* compressedAlphaIndices, void* decompressedBlock, int destinationPitch, int pixelSize) {
    unsigned char* decompressed;
    unsigned char alpha[8];
    int i, j;
    unsigned long long indices;

    decompressed = (unsigned char*) decompressedBlock;

    alpha[0] = compressedAlphas[0];
    alpha[1] = compressedAlphas[1];

    if (alpha[0] > alpha[1]) {
        /* 6 interpolated alpha values. */
        alpha[2] = (6 * alpha[0] +     alpha[1] + 1) / 7;   /* 6/7*alpha_0 + 1/7*alpha_1 */
        alpha[3] = (5 * alpha[0] + 2 * alpha[1] + 1) / 7;   /* 5/7*alpha_0 + 2/7*alpha_1 */
        alpha[4] = (4 * alpha[0] + 3 * alpha[1] + 1) / 7;   /* 4/7*alpha_0 + 3/7*alpha_1 */
        alpha[5] = (3 * alpha[0] + 4 * alpha[1] + 1) / 7;   /* 3/7*alpha_0 + 4/7*alpha_1 */
        alpha[6] = (2 * alpha[0] + 5 * alpha[1] + 1) / 7;   /* 2/7*alpha_0 + 5/7*alpha_1 */
        alpha[7] = (    alpha[0] + 6 * alpha[1] + 1) / 7;   /* 1/7*alpha_0 + 6/7*alpha_1 */
    }
    else {
        /* 4 interpolated alpha values. */
        alpha[2] = (4 * alpha[0] +     alpha[1] + 1) / 5;   /* 4/5*alpha_0 + 1/5*alpha_1 */
        alpha[3] = (3 * alpha[0] + 2 * alpha[1] + 1) / 5;   /* 3/5*alpha_0 + 2/5*alpha_1 */
        alpha[4] = (2 * alpha[0] + 3 * alpha[1] + 1) / 5;   /* 2/5*alpha_0 + 3/5*alpha_1 */
        alpha[5] = (    alpha[0] + 4 * alpha[1] + 1) / 5;   /* 1/5*alpha_0 + 4/5*alpha_1 */
        alpha[6] = 0x00;
        alpha[7] = 0xFF;
    }

    indices = *(unsigned long long*) compressedAlphaIndices;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * pixelSize] = alpha[indices & 0x07];
            indices >>= 3;
        }
        decompressed += destinationPitch;
    }
}

static void bcdec__all_alpha_255(void* decompressedBlock, int destinationPitch, int pixelSize) {
    int i, j;
    unsigned char* decompressed = (unsigned char*) decompressedBlock;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * pixelSize] = 255;
        }
        decompressed += destinationPitch;
    }
}

#if defined(_WIN32) || defined(_WIN64)
    // Windows-specific export
    #define EXPORT __declspec(dllexport)
#else
    // Linux and other POSIX-compliant OSes
    #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void bcdec_bc1(const unsigned char* compressedBlock, unsigned char* decompressedBlock, unsigned int blocksWidth, unsigned int blocksHeight, unsigned long compressedSize) {
    const unsigned short* compressedColors = (unsigned short*) compressedBlock;
    const unsigned int* compressedColorIndices = (unsigned int*) (compressedBlock + compressedSize/2);
    const int destinationPitch = blocksWidth*BLOCK_WIDTH*RGBA_SIZE;

    for (int i = 0; i < blocksHeight; i++) {
        for (int j = 0; j < blocksWidth; j++) {
            bcdec__color_block(compressedColors, compressedColorIndices, decompressedBlock, destinationPitch, 0);
            compressedColors += 2;
            compressedColorIndices += 1;
            decompressedBlock += 4*RGBA_SIZE;
        }
        decompressedBlock += blocksWidth*(BLOCK_HEIGHT-1)*4*RGBA_SIZE;
    }
}

EXPORT void bcdec_bc3(const unsigned char* compressedBlock, unsigned char* decompressedBlock, unsigned int blocksWidth, unsigned int blocksHeight, unsigned long compressedSize) {
    unsigned long compressedSizeFraction = compressedSize/8;
    const unsigned char* compressedAlphas = (unsigned char*) compressedBlock;
    const unsigned short* compressedColors = (unsigned short*) (compressedBlock + compressedSizeFraction*1);
    const unsigned char* compressedAlphaIndices = (unsigned char*) (compressedBlock + compressedSizeFraction*3);
    const unsigned int* compressedColorIndices = (unsigned int*) (compressedBlock + compressedSizeFraction*6);
    const int destinationPitch = blocksWidth*BLOCK_WIDTH*RGBA_SIZE;

    for (int i = 0; i < blocksHeight; i++) {
        for (int j = 0; j < blocksWidth; j++) {
            bcdec__color_block(compressedColors, compressedColorIndices, decompressedBlock, destinationPitch, 1);
            bcdec__smooth_alpha_block(compressedAlphas, compressedAlphaIndices, decompressedBlock + 3, destinationPitch, 4);
            compressedAlphas += 2;
            compressedColors += 2;
            compressedAlphaIndices += 6;
            compressedColorIndices += 1;
            decompressedBlock += 4*RGBA_SIZE;
        }
        decompressedBlock += blocksWidth*(BLOCK_HEIGHT-1)*4*RGBA_SIZE;
    }
}

EXPORT void bcdec_bc4(const unsigned char* compressedBlock, unsigned char* decompressedBlock, unsigned int blocksWidth, unsigned int blocksHeight, unsigned long compressedSize) {
    unsigned long compressedSizeFraction = compressedSize/8;
    const unsigned char* compressedAlphas = (unsigned char*) compressedBlock;
    const unsigned char* compressedAlphaIndices = (unsigned char*) (compressedBlock + compressedSizeFraction*2);
    const int destinationPitch = blocksWidth*BLOCK_WIDTH*RGBA_SIZE;

    for (int i = 0; i < blocksHeight; i++) {
        for (int j = 0; j < blocksWidth; j++) {
            bcdec__smooth_alpha_block(compressedAlphas, compressedAlphaIndices, decompressedBlock, destinationPitch, 4);
            bcdec__all_alpha_255(decompressedBlock + 3, destinationPitch, 4);
            compressedAlphas += 2;
            compressedAlphaIndices += 6;
            decompressedBlock += 4*RGBA_SIZE;
        }
        decompressedBlock += blocksWidth*(BLOCK_HEIGHT-1)*4*RGBA_SIZE;
    }
}

EXPORT void bcdec_bc5(const unsigned char* compressedBlock, unsigned char* decompressedBlock, unsigned int blocksWidth, unsigned int blocksHeight, unsigned long compressedSize) {
    unsigned long compressedSizeFraction = compressedSize/8;
    const unsigned char* compressedReds = (unsigned char*) compressedBlock;
    const unsigned char* compressedGreens = (unsigned char*) (compressedBlock + compressedSizeFraction*1);
    const unsigned char* compressedRedIndices = (unsigned char*) (compressedBlock + compressedSizeFraction*2);
    const unsigned char* compressedGreenIndices = (unsigned char*) (compressedBlock + compressedSizeFraction*5);
    const int destinationPitch = blocksWidth*BLOCK_WIDTH*RGBA_SIZE;

    for (int i = 0; i < blocksHeight; i++) {
        for (int j = 0; j < blocksWidth; j++) {
            bcdec__smooth_alpha_block(compressedReds, compressedRedIndices, decompressedBlock, destinationPitch, 4);
            bcdec__smooth_alpha_block(compressedGreens, compressedGreenIndices, decompressedBlock + 1, destinationPitch, 4);
            bcdec__all_alpha_255(decompressedBlock + 3, destinationPitch, 4);
            compressedReds += 2;
            compressedGreens += 2;
            compressedRedIndices += 6;
            compressedGreenIndices += 6;
            decompressedBlock += 4*RGBA_SIZE;
        }
        decompressedBlock += blocksWidth*(BLOCK_HEIGHT-1)*4*RGBA_SIZE;
    }
}

EXPORT void bcdec_r8(const unsigned char* compressedBlock, unsigned char* decompressedBlock, unsigned int blocksWidth, unsigned int blocksHeight, unsigned long compressedSize) {
    int offset;
    for (int i = 0; i < blocksHeight; i++) {
        for (int j = 0; j < blocksWidth; j++) {
            offset = i*blocksWidth + j;
            decompressedBlock[offset*4]= compressedBlock[offset];
            decompressedBlock[offset*4 + 3]= 255;
        }
    }
}