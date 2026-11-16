/*
Opus decoding test tool for dr_opus.

Decodes an Ogg Opus file with dr_opus and reports stream information, packet-loss
concealment counters and memory usage. Can dump the decoded PCM as raw s16/f32 for
comparison against a reference decoder (e.g. ffmpeg).

USAGE:
    dr_libs_test_opus <file.opus> [--s16 <out.raw>] [--f32 <out.raw>]

Exit code 0 on success, 1 on failure.
*/
#define DR_OPUS_IMPLEMENTATION
#include "../dr_opus.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    const char* pFilePath = NULL;
    const char* pOutS16 = NULL;
    const char* pOutF32 = NULL;
    int useMemory = 0;
    int i;
    dropus opus;
    FILE* pFileS16 = NULL;
    FILE* pFileF32 = NULL;
    dropus_uint64 totalFrames = 0;
    int exitCode = 1;
    unsigned char* pMemoryData = NULL;  /* --memory: backing block for dropus_init_memory(). */

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--s16") == 0 && i + 1 < argc) {
            pOutS16 = argv[++i];
        } else if (strcmp(argv[i], "--f32") == 0 && i + 1 < argc) {
            pOutF32 = argv[++i];
        } else if (strcmp(argv[i], "--memory") == 0) {
            useMemory = 1;
        } else if (argv[i][0] != '-') {
            pFilePath = argv[i];
        }
    }

    if (pFilePath == NULL) {
        fprintf(stderr, "USAGE: dr_libs_test_opus <file.opus> [--memory] [--s16 <out.raw>] [--f32 <out.raw>]\n");
        return 1;
    }

    if (useMemory) {
        /* Exercise dropus_init_memory(). dr_opus does not copy the block, so it must
        stay alive until after dropus_uninit(). */
        FILE* pFile = fopen(pFilePath, "rb");
        long size;
        dropus_bool32 ok = DROPUS_FALSE;
        if (pFile != NULL) {
            fseek(pFile, 0, SEEK_END);
            size = ftell(pFile);
            fseek(pFile, 0, SEEK_SET);
            pMemoryData = (unsigned char*)malloc((size_t)size);
            if (pMemoryData != NULL && fread(pMemoryData, 1, (size_t)size, pFile) == (size_t)size) {
                ok = dropus_init_memory(&opus, pMemoryData, (size_t)size, NULL);
            }
            fclose(pFile);
        }
        if (!ok) {
            fprintf(stderr, "Failed to initialize decoder from memory for: %s\n", pFilePath);
            free(pMemoryData);
            return 1;
        }
    } else if (!dropus_init_file(&opus, pFilePath, NULL)) {
        fprintf(stderr, "Failed to initialize decoder for: %s\n", pFilePath);
        return 1;
    }

    printf("format=opus version=%s channels=%u sampleRate=%u preSkip=%u outputGain=%d\n",
        dropus_version_string(), opus.channels, opus.sampleRate, opus.preSkipInPCMFrames, (int)opus.outputGain);
    printf("sizeof(dropus)=%u opusDecoderState=%u\n",
        (unsigned int)sizeof(dropus), (unsigned int)dropus_opus_decoder_state_size((int)opus.channels));

    if (pOutS16 != NULL) {
        pFileS16 = fopen(pOutS16, "wb");
    }
    if (pOutF32 != NULL) {
        pFileF32 = fopen(pOutF32, "wb");
    }

    for (;;) {
        float buffer[4096 * 2];
        dropus_uint64 framesRead = dropus_read_pcm_frames_f32(&opus, 4096, buffer);
        if (framesRead == 0) {
            break;
        }
        totalFrames += framesRead;
        if (pFileF32 != NULL) {
            fwrite(buffer, sizeof(float), (size_t)framesRead * opus.channels, pFileF32);
        }
        if (pFileS16 != NULL) {
            dropus_int16 bufferS16[4096 * 2];
            dropus_uint64 iFrame;
            dropus_uint64 sampleCount = framesRead * opus.channels;
            for (iFrame = 0; iFrame < sampleCount; ++iFrame) {
                float x = buffer[iFrame] * 32768.0f;
                int s = (int)((x >= 0.0f) ? x + 0.5f : x - 0.5f);
                if (s > 32767) {
                    s = 32767;
                }
                if (s < -32768) {
                    s = -32768;
                }
                bufferS16[iFrame] = (dropus_int16)s;
            }
            fwrite(bufferS16, sizeof(dropus_int16), (size_t)sampleCount, pFileS16);
        }
    }

    printf("pcmFrames=%llu concealedFrames=%llu lostPackets=%llu resyncedPages=%llu atEnd=%s\n",
        (unsigned long long)totalFrames,
        (unsigned long long)opus.totalPCMFramesConcealed,
        (unsigned long long)opus.totalPacketsLost,
        (unsigned long long)opus.totalPagesResynced,
        opus.atEnd ? "yes" : "no");

    exitCode = 0;

    if (pFileS16 != NULL) {
        fclose(pFileS16);
    }
    if (pFileF32 != NULL) {
        fclose(pFileF32);
    }
    dropus_uninit(&opus);
    free(pMemoryData);
    return exitCode;
}
