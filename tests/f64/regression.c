/* Regression check for the pre-existing f32/s16/s32 read paths. This is
   compiled against both the original and the patched dr_wav.h by run.sh and
   the two outputs are diffed; they must be identical. */
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include <stdio.h>

int main(void)
{
    unsigned int ch = 0, sr = 0;
    drwav_uint64 frames = 0, i;
    float*  f = drwav_open_file_and_read_pcm_frames_f32("output/f64_stereo.wav", &ch, &sr, &frames, NULL);
    drwav_int16* s = drwav_open_file_and_read_pcm_frames_s16("output/f64_stereo.wav", &ch, &sr, &frames, NULL);
    drwav_int32* g = drwav_open_file_and_read_pcm_frames_s32("output/f64_stereo.wav", &ch, &sr, &frames, NULL);
    if (f == NULL || s == NULL || g == NULL) {
        printf("regression: read failed\n");
        return 1;
    }
    printf("ch=%u sr=%u frames=%llu\n", ch, sr, (unsigned long long)frames);
    for (i = 0; i < 8; ++i) { printf("f32[%llu]=%.9g\n", (unsigned long long)i, f[i]); }
    for (i = 0; i < 8; ++i) { printf("s16[%llu]=%d\n",   (unsigned long long)i, s[i]); }
    for (i = 0; i < 8; ++i) { printf("s32[%llu]=%d\n",   (unsigned long long)i, g[i]); }
    drwav_free(f, NULL);
    drwav_free(s, NULL);
    drwav_free(g, NULL);
    return 0;
}
