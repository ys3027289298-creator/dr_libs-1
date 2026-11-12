#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define RATE 8000
#define N_CLIPS 4

static const char* g_names[N_CLIPS] = { "sine_1s", "silence_0p5s", "odd_samples", "zero_frames" };

static drwav_int16* make_pcm(int clip, drwav_uint64* pCount)
{
    drwav_uint64 i, n = (drwav_uint64[]){ RATE, RATE / 2, 2037, 0 }[clip];
    drwav_int16* p = (drwav_int16*)malloc((n ? n : 1) * sizeof(drwav_int16));
    if (p == NULL) {
        exit(2);
    }
    for (i = 0; i < n; ++i) {
        if (clip == 0) {
            p[i] = (drwav_int16)(20000.0 * sin(2.0 * 3.14159265358979323846 * 440.0 * (double)i / RATE));
        } else {
            p[i] = 0;
        }
    }
    *pCount = n;
    return p;
}

static long file_size(const char* path)
{
    FILE* f = fopen(path, "rb");
    long size;
    if (f == NULL) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
    return size;
}

int main(int argc, char** argv)
{
    const char* outDir = (argc > 1) ? argv[1] : "output";
    int clip;

    for (clip = 0; clip < N_CLIPS; ++clip) {
        char pcmPath[512], adpcmPath[512];
        drwav_data_format fmt;
        drwav wavPcm, wavEnc, wavDec;
        drwav_uint64 frameCount, framesWritten, framesRead;
        drwav_int16* pcm;
        drwav_int16 decoded[16];
        long pcmSize, adpcmSize;
        int i;

        snprintf(pcmPath, sizeof(pcmPath), "%s/%s_pcm.wav", outDir, g_names[clip]);
        snprintf(adpcmPath, sizeof(adpcmPath), "%s/%s_adpcm.wav", outDir, g_names[clip]);
        pcm = make_pcm(clip, &frameCount);

        fmt.container = drwav_container_riff;
        fmt.channels = 1;
        fmt.sampleRate = RATE;
        fmt.bitsPerSample = 16;
        fmt.format = DR_WAVE_FORMAT_PCM;
        if (!drwav_init_file_write(&wavPcm, pcmPath, &fmt, NULL)) {
            printf("%s: PCM writer init failed\n", g_names[clip]);
            free(pcm);
            continue;
        }
        drwav_write_pcm_frames(&wavPcm, frameCount, pcm);
        drwav_uninit(&wavPcm);

        fmt.bitsPerSample = 4;
        fmt.format = DR_WAVE_FORMAT_DVI_ADPCM;
        if (!drwav_init_file_write_ima_adpcm_sequential_pcm_frames(&wavEnc, adpcmPath, &fmt, frameCount, NULL)) {
            printf("%s: IMA ADPCM writer init failed\n", g_names[clip]);
            free(pcm);
            continue;
        }
        framesWritten = drwav_write_pcm_frames(&wavEnc, frameCount, pcm);
        drwav_uninit(&wavEnc);

        printf("file=%s sample_rate=%u\n", g_names[clip], (unsigned)RATE);
        printf("  input_frames=%llu encoded_frames_written=%llu\n",
            (unsigned long long)frameCount, (unsigned long long)framesWritten);

        if (!drwav_init_file(&wavDec, adpcmPath, NULL)) {
            printf("  dr_wav_readback: open failed\n\n");
            free(pcm);
            continue;
        }
        framesRead = drwav_read_pcm_frames_s16(&wavDec, 16, decoded);
        printf("  drwav_reported_frames=%llu (input=%llu; last block is zero-padded, dr_wav counts padding)\n",
            (unsigned long long)wavDec.totalPCMFrameCount, (unsigned long long)frameCount);
        printf("  first16_relative_error:");
        for (i = 0; i < (int)framesRead; ++i) {
            double denom = (double)(abs(pcm[i]) > 1 ? abs(pcm[i]) : 1);
            printf(" %0.6f", fabs((double)decoded[i] - (double)pcm[i]) / denom);
        }
        printf("\n");
        drwav_uninit(&wavDec);

        pcmSize = file_size(pcmPath);
        adpcmSize = file_size(adpcmPath);
        printf("  size_pcm=%ld bytes size_adpcm=%ld bytes ratio=%.3f\n\n",
            pcmSize, adpcmSize, pcmSize > 0 ? (double)adpcmSize / pcmSize : 0.0);
        free(pcm);
    }

    return 0;
}
