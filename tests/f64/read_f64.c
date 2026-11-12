/* Reads the 64-bit float WAV files in output/ and verifies them against the
   theoretical values written by gen_f64.py. Build from the repo root:
   gcc -I. tests/f64/read_f64.c -o output/read_f64 -lm */
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include <stdio.h>
#include <math.h>

#define NFRAMES 16
#define SAMPLE_RATE 44100

static double expected(long k) { return (double)(k % 17) * 0.1 - 0.8; }

static int check(const char* path, int expect_ok)
{
    drwav wav;
    double buf[8];
    double max_err = 0;
    drwav_uint64 frames_read, i;
    int meta_ok;
    printf("file: %s\n", path);
    if (!drwav_init_file(&wav, path, NULL)) {
        printf("  init failed (%s)\n", expect_ok ? "UNEXPECTED" : "expected");
        return expect_ok ? 1 : 0;
    }
    if (!expect_ok) {
        printf("  init unexpectedly succeeded\n");
        drwav_uninit(&wav);
        return 1;
    }
    meta_ok = (wav.sampleRate == SAMPLE_RATE && wav.totalPCMFrameCount == NFRAMES);
    printf("  sampleRate=%u channels=%u frames=%llu duration=%.6fs\n",
        wav.sampleRate, wav.channels, (unsigned long long)wav.totalPCMFrameCount,
        (double)wav.totalPCMFrameCount / wav.sampleRate);
    frames_read = drwav_read_pcm_frames_f64(&wav, 8 / wav.channels, buf);
    printf("  first samples:");
    for (i = 0; i < frames_read * wav.channels; ++i) {
        double err = fabs(buf[i] - expected((long)i));
        if (err > max_err) { max_err = err; }
        printf(" %.6f", buf[i]);
    }
    printf("\n  maxError=%.3e (%s)\n", max_err, (max_err < 1e-12 && meta_ok) ? "ok" : "FAIL");
    drwav_uninit(&wav);
    return (max_err < 1e-12 && meta_ok) ? 0 : 1;
}

int main(void)
{
    int fails = 0;
    fails += check("output/f64_stereo.wav", 1);
    fails += check("output/f64_mono.wav", 1);
    fails += check("output/f64_odd_tail.wav", 1);
    fails += check("output/f64_bad_header.wav", 0);
    printf("RESULT: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
