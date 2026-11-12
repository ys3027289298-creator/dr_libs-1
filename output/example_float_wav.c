#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static long file_size(const char* path) {
    FILE* f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <data.txt> <out.wav> [int16]\n", argv[0]);
        return 2;
    }
    FILE* in = fopen(argv[1], "r");
    if (!in) { perror("data"); return 1; }
    unsigned int ch, frames, rate;
    if (fscanf(in, "%u %u %u", &ch, &frames, &rate) != 3) { return 1; }
    size_t count = (size_t)ch * frames;
    float* samples = malloc(count * sizeof(float));
    for (size_t i = 0; i < count; i++) {
        if (fscanf(in, "%f", &samples[i]) != 1) { return 1; }
    }
    fclose(in);

    drwav_data_format fmt;
    DRWAV_ZERO_OBJECT(&fmt);
    fmt.container = drwav_container_riff;
    fmt.channels = ch;
    fmt.sampleRate = rate;
    int integer_path = (argc >= 4);
    if (integer_path) {
        fmt.format = DR_WAVE_FORMAT_PCM;
        fmt.bitsPerSample = 16;
    } else {
        fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
        fmt.bitsPerSample = 32;
    }

    drwav wav;
    if (!drwav_init_file_write(&wav, argv[2], &fmt, NULL)) {
        fprintf(stderr, "init write failed\n");
        return 1;
    }
    if (integer_path) {
        drwav_int16* pcm = malloc(count * sizeof(drwav_int16));
        drwav_f32_to_s16(pcm, samples, count);
        drwav_write_pcm_frames(&wav, frames, pcm);
        free(pcm);
    } else {
#ifdef DRWAV_HAS_WRITE_F32
        drwav_write_pcm_frames_f32(&wav, frames, samples);
#else
        fprintf(stderr, "this build lacks drwav_write_pcm_frames_f32\n");
        return 1;
#endif
    }
    drwav_uninit(&wav);

    if (!drwav_init_file(&wav, argv[2], NULL)) {
        fprintf(stderr, "init read failed\n");
        return 1;
    }
    float* readback = malloc(count * sizeof(float));
    drwav_uint64 got = drwav_read_pcm_frames_f32(&wav, frames, readback);
    drwav_uninit(&wav);

    double max_err = 0.0;
    for (size_t i = 0; i < count; i++) {
        double e = fabs((double)readback[i] - (double)samples[i]);
        if (e > max_err) max_err = e;
    }
    printf("frames=%llu channels=%u tag=%s max_error=%.9g file_bytes=%ld\n",
           (unsigned long long)got, ch,
           integer_path ? "PCM(1)" : "FLOAT(3)", max_err, file_size(argv[2]));
    free(samples);
    free(readback);
    return 0;
}
