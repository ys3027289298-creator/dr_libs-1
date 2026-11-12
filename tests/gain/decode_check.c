#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        drmp3 mp3;
        drmp3_uint64 total = 0;
        drmp3_uint64 got;
        float buffer[4096];

        if (!drmp3_init_file(&mp3, argv[i], NULL)) {
            printf("%s INIT_FAIL\n", argv[i]);
            continue;
        }

        while ((got = drmp3_read_pcm_frames_f32(&mp3, 1024, buffer)) > 0) {
            total += got;
        }

        printf("%s decode_ok channels=%u rate=%u pcm_frames=%llu\n",
               argv[i], mp3.channels, mp3.sampleRate, (unsigned long long)total);
        drmp3_uninit(&mp3);
    }

    return 0;
}
