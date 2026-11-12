/* Frame-level MPEG-1 Layer II verification for dr_mp3.
 * Decodes each file one MP2 frame at a time through drmp3dec_decode_frame(),
 * prints the file name, sample rate, channel count, decoded frame count and the
 * first 8 PCM samples, then compares the whole stream against an f32le reference
 * waveform and reports the maximum absolute sample error.
 *
 * Usage: layer2_test <file.mp2> <reference.f32> [expect_corrupt]
 * Exit code is 0 when every decoded frame matches the reference within tolerance.
 */
/* The low level push API outputs int16 by default; ask for float so the frames
 * can be compared directly against the ffmpeg f32le reference waveform. */
#ifndef DR_MP3_FLOAT_OUTPUT
#define DR_MP3_FLOAT_OUTPUT
#endif
#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_FRAME_SAMPLES 1152
#define MAX_CHANNELS      2
#define MAX_TOTAL_SAMPLES (64 * 1024)
#define TOLERANCE         0.0002f   /* float32 quantisation margin over the ffmpeg reference */

static unsigned char *read_file(const char *path, long *size_out)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    *size_out = ftell(f);
    rewind(f);
    unsigned char *buf = (unsigned char*)malloc((size_t)*size_out);
    if (buf != NULL && fread(buf, 1, (size_t)*size_out, f) != (size_t)*size_out) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

static float *read_f32(const char *path, long *count_out)
{
    long size;
    unsigned char *raw = read_file(path, &size);
    if (raw == NULL) {
        return NULL;
    }
    *count_out = size / (long)sizeof(float);
    return (float*)raw;
}

int main(int argc, char **argv)
{
    long mp3_size;
    unsigned char *mp3;
    drmp3dec decoder;
    drmp3dec_frame_info info;
    float frame_pcm[MAX_FRAME_SAMPLES * MAX_CHANNELS];
    float *decoded = (float*)malloc(sizeof(float) * MAX_TOTAL_SAMPLES);
    float *reference;
    long ref_count;
    int pos = 0, frames = 0, bad_frames = 0, sample_count = 0;
    int expect_corrupt = argc > 3;
    int first_header_printed = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <file.mp2> <reference.f32> [expect_corrupt]\n", argv[0]);
        return 2;
    }

    mp3 = read_file(argv[1], &mp3_size);
    reference = read_f32(argv[2], &ref_count);
    if (mp3 == NULL || reference == NULL || decoded == NULL) {
        fprintf(stderr, "%s: failed to load inputs\n", argv[1]);
        return 2;
    }

    drmp3dec_init(&decoder);
    while (pos < mp3_size) {
        int samples = drmp3dec_decode_frame(&decoder, mp3 + pos, (int)(mp3_size - pos), frame_pcm, &info);
        if (info.frame_bytes == 0) {
            break;  /* no frame header found / need more data */
        }
        if (samples == 0) {
            bad_frames++;
        } else {
            int produced = samples * info.channels;
            if (!first_header_printed) {
                printf("file=%s sample_rate=%uHz channels=%u\n", argv[1], info.sample_rate, info.channels);
                first_header_printed = 1;
            }
            for (int i = 0; i < produced && sample_count < MAX_TOTAL_SAMPLES; i++) {
                decoded[sample_count++] = frame_pcm[i];
            }
            frames++;
        }
        pos += info.frame_bytes;
    }

    printf("frames=%d bad_frames=%d first_8_samples=", frames, bad_frames);
    for (int i = 0; i < 8; i++) {
        printf("%s%.6f", i ? " " : "", i < sample_count ? decoded[i] : 0.0f);
    }
    printf("\n");

    double max_error = 0.0;
    long compare = (long)sample_count < ref_count ? sample_count : ref_count;
    for (long i = 0; i < compare; i++) {
        double e = fabs((double)decoded[i] - (double)reference[i]);
        if (e > max_error) {
            max_error = e;
        }
    }
    printf("reference_samples=%ld compared=%ld max_error=%.8f\n", ref_count, compare, max_error);

    int ok = max_error <= TOLERANCE && frames > 0 &&
             (!expect_corrupt || bad_frames > 0);
    printf("result=%s\n\n", ok ? "PASS" : "FAIL");

    free(mp3);
    free(reference);
    free(decoded);
    return ok ? 0 : 1;
}
