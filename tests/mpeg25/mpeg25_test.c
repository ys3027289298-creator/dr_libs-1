#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Per-frame MPEG-2.5 probe: prints name, rate, frame count and first 8 samples. */
int main(int argc, char** argv)
{
    FILE* pFile;
    long fileSize;
    unsigned char* pData;
    drmp3dec decoder;
    drmp3dec_frame_info info;
    drmp3_int16 pcm[1152 * 2];
    int offset = 0, frameCount = 0, expectFail = 0;
    const char* pDumpPath = NULL;

    if (argc < 2) {
        return 2;
    }

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--bad") == 0) {
            expectFail = 1;
        } else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            pDumpPath = argv[++i];
        }
    }

    pFile = fopen(argv[1], "rb");
    if (pFile == NULL) {
        return 2;
    }

    fseek(pFile, 0, SEEK_END);
    fileSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);
    pData = (unsigned char*)malloc((size_t)fileSize);
    if (pData == NULL || (long)fread(pData, 1, (size_t)fileSize, pFile) != fileSize) {
        fclose(pFile);
        free(pData);
        return 2;
    }
    fclose(pFile);

    drmp3dec_init(&decoder);
    FILE* pDump = NULL;
    if (pDumpPath != NULL) {
        pDump = fopen(pDumpPath, "wb");
    }

    while (offset + DRMP3_HDR_SIZE <= (int)fileSize) {
        int samples = drmp3dec_decode_frame(&decoder, pData + offset, (int)fileSize - offset, pcm, &info);
        if (info.frame_bytes <= 0) {
            break;
        }
        if (samples <= 0) {
            break;   /* No PCM produced: invalid/unsupported frame. */
        }

        if (pDump != NULL) {
            fwrite(pcm, sizeof(drmp3_int16), (size_t)samples * info.channels, pDump);
        }

        if (frameCount == 0) {
            printf("file       : %s\n", argv[1]);
            printf("sample rate: %d Hz\n", info.sample_rate);
            printf("channels   : %d\n", info.channels);
            printf("first 8    :");
            for (int i = 0; i < 8 && i < samples * info.channels; ++i) {
                printf(" %d", pcm[i]);
            }
            printf("\n");
        }

        offset    += info.frame_bytes;
        frameCount += 1;
    }

    printf("frame count: %d\n", frameCount);
    if (pDump != NULL) {
        fclose(pDump);
    }
    free(pData);

    if (expectFail) {
        return frameCount == 0 ? 0 : 3;   /* Corrupt headers must not produce frames. */
    }

    return frameCount > 0 ? 0 : 1;
}
