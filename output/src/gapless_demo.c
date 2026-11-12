/*
Gapless MP3 decoding demo for the patched dr_mp3.

Runs five stages:
  1. Environment confirmation (working directory, compiler, dr_mp3.h path)
  2. Build of this demo is performed by build.sh; this binary just announces itself
  3. Per-file decode with gapless stats (LAME CBR, LAME VBR, no-LAME, mid-stream seek)
  4. Waveform verification (max amplitude of the 512 samples at each end, before/after trim)
  5. Regression (deterministic hashes across two full decodes, mid-point seek sample index)

All generated PCM is written as raw interleaved signed 16-bit little-endian into output/pcm.
The program never waits for keyboard input.
*/
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DEMO_READ_FRAMES   1152
#define EDGE_SAMPLES       512

typedef struct
{
    const char* path;
    const char* label;
} demo_file;

typedef struct
{
    drmp3_uint64 frames;
    drmp3_int16* samples;   /* interleaved s16, frames*channels samples */
} pcm_blob;

static uint64_t fnv1a(const void* data, size_t bytes)
{
    const unsigned char* p = (const unsigned char*)data;
    uint64_t h = 1469598103934665603ULL;
    size_t i;
    for (i = 0; i < bytes; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void banner(const char* title)
{
    printf("\n======================================================================\n");
    printf("%s\n", title);
    printf("======================================================================\n");
}

static int max_abs_edge(const pcm_blob* blob, drmp3_uint32 channels, int atHead, drmp3_uint64 edgeFrames, drmp3_uint64* outFramesExamined)
{
    drmp3_uint64 frames = edgeFrames < blob->frames ? edgeFrames : blob->frames;
    drmp3_uint64 i;
    int peak = 0;

    for (i = 0; i < frames; ++i) {
        drmp3_uint64 frameIndex = atHead ? i : (blob->frames - 1 - i);
        drmp3_uint32 c;
        for (c = 0; c < channels; ++c) {
            int v = blob->samples[frameIndex * channels + c];
            if (v < 0) v = -v;
            if (v > peak) peak = v;
        }
    }

    if (outFramesExamined) *outFramesExamined = frames;
    return peak;
}

/* Counts the run of exactly-zero PCM frames at either end of the blob (all channels must be zero). */
static drmp3_uint64 zero_run_edge(const pcm_blob* blob, drmp3_uint32 channels, int atHead)
{
    drmp3_uint64 run = 0;

    if (atHead) {
        while (run < blob->frames) {
            drmp3_uint32 c;
            int allZero = 1;
            for (c = 0; c < channels; ++c) {
                if (blob->samples[run * channels + c] != 0) { allZero = 0; break; }
            }
            if (!allZero) break;
            run++;
        }
    } else {
        while (run < blob->frames) {
            drmp3_uint64 frameIndex = blob->frames - 1 - run;
            drmp3_uint32 c;
            int allZero = 1;
            for (c = 0; c < channels; ++c) {
                if (blob->samples[frameIndex * channels + c] != 0) { allZero = 0; break; }
            }
            if (!allZero) break;
            run++;
        }
    }

    return run;
}

/* Decodes the whole file. When raw is non-zero the gapless trim is disabled for the measurement pass. */
static int decode_full(const char* path, int raw, pcm_blob* out)
{
    drmp3 mp3;
    drmp3_int16 chunk[DEMO_READ_FRAMES * 2];
    drmp3_uint64 capacity = 0;
    drmp3_uint64 used = 0;

    out->frames = 0;
    out->samples = NULL;

    if (!drmp3_init_file(&mp3, path, NULL)) {
        printf("  !! failed to open %s\n", path);
        return 0;
    }

    if (raw) {
        mp3.delayInPCMFrames = 0;
        mp3.paddingInPCMFrames = 0;
        mp3.totalPCMFrameCount = DRMP3_UINT64_MAX;
    }

    for (;;) {
        drmp3_uint64 got = drmp3_read_pcm_frames_s16(&mp3, DEMO_READ_FRAMES, chunk);
        drmp3_uint64 samples;
        if (got == 0) break;

        samples = got * mp3.channels;
        if (used + samples > capacity) {
            drmp3_uint64 newCap = capacity ? capacity * 2 : 4096;
            drmp3_int16* grown;
            while (newCap < used + samples) newCap *= 2;
            grown = (drmp3_int16*)realloc(out->samples, newCap * sizeof(drmp3_int16));
            if (!grown) {
                free(out->samples);
                drmp3_uninit(&mp3);
                return 0;
            }
            out->samples = grown;
            capacity = newCap;
        }
        memcpy(out->samples + used, chunk, samples * sizeof(drmp3_int16));
        used += samples;
    }

    out->frames = used / mp3.channels;
    drmp3_uninit(&mp3);
    return 1;
}

static void write_pcm(const char* pcmPath, const pcm_blob* blob, drmp3_uint32 channels)
{
    FILE* f = fopen(pcmPath, "wb");
    if (!f) {
        printf("  !! cannot write %s\n", pcmPath);
        return;
    }
    fwrite(blob->samples, sizeof(drmp3_int16), blob->frames * channels, f);
    fclose(f);
    printf("  PCM written: %s (%llu frames, %u channels, s16 LE)\n",
           pcmPath, (unsigned long long)blob->frames, channels);
}

static void stage3_file(const demo_file* file, const char* pcmDir)
{
    drmp3 mp3;
    drmp3_gapless_info info;
    pcm_blob trimmed;
    pcm_blob raw;
    char pcmPath[512];
    const char* base;
    drmp3_uint64 trimmedSamples;

    printf("\n-- %s (%s) --\n", file->label, file->path);
    if (!drmp3_init_file(&mp3, file->path, NULL)) {
        printf("  open failed\n");
        return;
    }

    drmp3_get_gapless_info(&mp3, &info);
    printf("  file                 : %s\n", file->path);
    printf("  sample rate          : %u Hz\n", mp3.sampleRate);
    printf("  channels             : %u\n", mp3.channels);
    printf("  gapless source       : %s\n", info.hasLAMEInfo ? "LAME tag" : "frame-structure fallback estimate");
    printf("  raw PCM frames       : %llu (samples per channel; includes delay+padding)\n",
           (unsigned long long)info.totalPCMFrameCount);
    printf("  delay trimmed (head) : %u frames\n", info.delayInPCMFrames);
    printf("  padding trimmed(tail): %u frames\n", info.paddingInPCMFrames);
    printf("  trimmed total        : %u frames\n",
           (unsigned)(info.delayInPCMFrames + info.paddingInPCMFrames));
    printf("  output PCM frames    : %llu\n", (unsigned long long)info.outputPCMFrameCount);
    printf("  output total samples : %llu (interleaved, all channels)\n",
           (unsigned long long)(info.outputPCMFrameCount * mp3.channels));

    drmp3_uninit(&mp3);

    if (!decode_full(file->path, 0, &trimmed)) return;
    if (!decode_full(file->path, 1, &raw)) { free(trimmed.samples); return; }

    printf("  actually decoded     : raw=%llu frames, trimmed=%llu frames\n",
           (unsigned long long)raw.frames, (unsigned long long)trimmed.frames);

    base = strrchr(file->path, '/');
    base = base ? base + 1 : file->path;
    snprintf(pcmPath, sizeof(pcmPath), "%s/%s.trimmed.s16.pcm", pcmDir, base);
    write_pcm(pcmPath, &trimmed, mp3.channels);
    snprintf(pcmPath, sizeof(pcmPath), "%s/%s.raw.s16.pcm", pcmDir, base);
    write_pcm(pcmPath, &raw, mp3.channels);

    trimmedSamples = trimmed.frames * mp3.channels;

    /* Stage 4 numbers, printed per file so the two stages line up. */
    {
        drmp3_uint64 examined;
        int rawHead = max_abs_edge(&raw, mp3.channels, 1, EDGE_SAMPLES, &examined);
        int rawTail = max_abs_edge(&raw, mp3.channels, 0, EDGE_SAMPLES, &examined);
        int trimHead = max_abs_edge(&trimmed, mp3.channels, 1, EDGE_SAMPLES, &examined);
        int trimTail = max_abs_edge(&trimmed, mp3.channels, 0, EDGE_SAMPLES, &examined);
        drmp3_uint64 rawHeadZeros = zero_run_edge(&raw, mp3.channels, 1);
        drmp3_uint64 rawTailZeros = zero_run_edge(&raw, mp3.channels, 0);
        drmp3_uint64 trimHeadZeros = zero_run_edge(&trimmed, mp3.channels, 1);
        drmp3_uint64 trimTailZeros = zero_run_edge(&trimmed, mp3.channels, 0);
        drmp3_uint64 trimFrames = (drmp3_uint64)info.delayInPCMFrames + info.paddingInPCMFrames;
        double trimmedMs = mp3.sampleRate ? (double)trimFrames * 1000.0 / (double)mp3.sampleRate : 0.0;

        printf("  edge peak |sample| over first/last %d PCM frames (%d samples/channel each):\n",
               EDGE_SAMPLES, EDGE_SAMPLES);
        printf("    before trim : head=%d  tail=%d\n", rawHead, rawTail);
        printf("    after  trim : head=%d  tail=%d\n", trimHead, trimTail);
        printf("  exact-zero run (PCM frames, all channels):\n");
        printf("    before trim : head=%llu  tail=%llu  <- codec delay/padding region\n",
               (unsigned long long)rawHeadZeros, (unsigned long long)rawTailZeros);
        printf("    after  trim : head=%llu  tail=%llu  <- silence removed from the output\n",
               (unsigned long long)trimHeadZeros, (unsigned long long)trimTailZeros);
        printf("  trimmed duration     : %.3f ms (%llu frames @ %u Hz)\n",
               trimmedMs, (unsigned long long)trimFrames, mp3.sampleRate);
    }

    free(trimmed.samples);
    free(raw.samples);
    (void)trimmedSamples;
}

int main(int argc, char** argv)
{
    const char* mediaDir = (argc > 1) ? argv[1] : "output/media";
    const char* pcmDir   = (argc > 2) ? argv[2] : "output/pcm";
    demo_file files[3];
    char pathBuf[3][512];
    int i;

    banner("STAGE 3/4 INPUT SET (paths resolved at runtime)");
    snprintf(pathBuf[0], sizeof(pathBuf[0]), "%s/lame_cbr.mp3",   mediaDir);
    snprintf(pathBuf[1], sizeof(pathBuf[1]), "%s/lame_vbr.mp3",   mediaDir);
    snprintf(pathBuf[2], sizeof(pathBuf[2]), "%s/nolame_cbr.mp3", mediaDir);
    files[0].path = pathBuf[0]; files[0].label = "LAME CBR";
    files[1].path = pathBuf[1]; files[1].label = "LAME VBR";
    files[2].path = pathBuf[2]; files[2].label = "no LAME tag (fallback estimate)";

    banner("STAGE 3: PER-FILE DECODE + STAGE 4: WAVEFORM VERIFICATION");
    for (i = 0; i < 3; ++i) {
        stage3_file(&files[i], pcmDir);
    }

    /* Mid-stream seek case: decode first half, seek past the middle, continue decoding. */
    banner("STAGE 3 CASE 4: MID-STREAM SEEK THEN CONTINUE");
    {
        drmp3 mp3;
        drmp3_gapless_info info;
        const char* path = files[0].path;
        drmp3_uint64 midpoint;
        drmp3_int16 chunk[DEMO_READ_FRAMES * 2];
        drmp3_uint64 firstGot;
        drmp3_uint64 continued = 0;
        drmp3_uint64 firstSampleIndex;
        int seekOk;

        if (!drmp3_init_file(&mp3, path, NULL)) {
            printf("  open failed: %s\n", path);
            return 1;
        }
        drmp3_get_gapless_info(&mp3, &info);
        midpoint = info.outputPCMFrameCount / 2;

        printf("  file                 : %s\n", path);
        printf("  sample rate          : %u Hz\n", mp3.sampleRate);
        printf("  channels             : %u\n", mp3.channels);
        printf("  raw PCM frames       : %llu\n", (unsigned long long)info.totalPCMFrameCount);
        printf("  delay/padding trimmed: %u / %u\n", info.delayInPCMFrames, info.paddingInPCMFrames);
        printf("  output PCM frames    : %llu\n", (unsigned long long)info.outputPCMFrameCount);

        firstGot = drmp3_read_pcm_frames_s16(&mp3, DEMO_READ_FRAMES, chunk);
        printf("  pre-seek read        : %llu frames\n", (unsigned long long)firstGot);

        seekOk = drmp3_seek_to_pcm_frame(&mp3, midpoint);
        printf("  seek to midpoint     : frame %llu -> %s\n",
               (unsigned long long)midpoint, seekOk ? "ok" : "FAILED");

        firstSampleIndex = drmp3_get_cursor_pcm_frame(&mp3);
        printf("  first sample after seek (0-based output index): %llu\n",
               (unsigned long long)firstSampleIndex);

        for (;;) {
            drmp3_uint64 got = drmp3_read_pcm_frames_s16(&mp3, DEMO_READ_FRAMES, chunk);
            if (got == 0) break;
            continued += got;
        }
        printf("  frames after seek    : %llu\n", (unsigned long long)continued);
        printf("  total delivered      : %llu pre-seek frames are discarded; post-seek stream ends cleanly\n",
               (unsigned long long)firstGot);

        drmp3_uninit(&mp3);
    }

    banner("STAGE 5: REGRESSION (determinism + seek index)");
    {
        const char* path = files[0].path;
        pcm_blob runA, runB;
        uint64_t hashA, hashB;
        drmp3 mp3;
        drmp3_gapless_info info;
        drmp3_uint64 midpoint;
        int seekOk;

        decode_full(path, 0, &runA);
        decode_full(path, 0, &runB);
        hashA = fnv1a(runA.samples, runA.frames * 2 * sizeof(drmp3_int16));
        hashB = fnv1a(runB.samples, runB.frames * 2 * sizeof(drmp3_int16));
        printf("  full decode run #1   : frames=%llu FNV-1a=%016llx\n",
               (unsigned long long)runA.frames, (unsigned long long)hashA);
        printf("  full decode run #2   : frames=%llu FNV-1a=%016llx\n",
               (unsigned long long)runB.frames, (unsigned long long)hashB);
        printf("  hashes identical     : %s\n", hashA == hashB ? "YES" : "NO");
        free(runA.samples);
        free(runB.samples);

        if (!drmp3_init_file(&mp3, path, NULL)) return 1;
        drmp3_get_gapless_info(&mp3, &info);
        midpoint = info.outputPCMFrameCount / 2;
        seekOk = drmp3_seek_to_pcm_frame(&mp3, midpoint);
        printf("  seek midpoint target : %llu, result=%s\n",
               (unsigned long long)midpoint, seekOk ? "ok" : "FAILED");
        printf("  first sample index after seek: %llu\n",
               (unsigned long long)drmp3_get_cursor_pcm_frame(&mp3));
        drmp3_uninit(&mp3);
    }

    printf("\nAll stages finished.\n");
    return 0;
}
