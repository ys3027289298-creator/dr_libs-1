#define DR_WAV_IMPLEMENTATION
#include "dr_wav_baseline.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define RATE 8000

static void write_clip(const char* dir, const char* name, drwav_uint64 n, int sine)
{
    char path[512];
    drwav_data_format fmt;
    drwav wav;
    drwav_int16* p = (drwav_int16*)malloc((n ? n : 1) * sizeof(drwav_int16));
    drwav_uint64 i;
    for (i = 0; i < n; ++i) {
        p[i] = sine ? (drwav_int16)(20000.0 * sin(2.0 * 3.14159265358979 * 440.0 * (double)i / RATE)) : 0;
    }
    snprintf(path, sizeof(path), "%s/%s_pcm.wav", dir, name);
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.channels = 1;
    fmt.sampleRate = RATE;
    fmt.bitsPerSample = 16;
    drwav_init_file_write(&wav, path, &fmt, NULL);
    drwav_write_pcm_frames(&wav, n, p);
    drwav_uninit(&wav);
    free(p);
}

int main(int argc, char** argv)
{
    const char* dir = (argc > 1) ? argv[1] : ".";
    write_clip(dir, "sine_1s", RATE, 1);
    write_clip(dir, "silence_0p5s", RATE / 2, 0);
    write_clip(dir, "odd_samples", 2037, 0);
    write_clip(dir, "zero_frames", 0, 0);
    return 0;
}
