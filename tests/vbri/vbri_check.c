#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"
#include <stdio.h>
#include <stdlib.h>

/* Reads each MP3 argument and prints name, MP3 frame count, duration in seconds and average bitrate. */
int main(int argc, char** argv)
{
    int i;
    for (i = 1; i < argc; ++i) {
        long size;
        char* data;
        drmp3 mp3;
        drmp3_uint64 frames, pcm;
        FILE* f = fopen(argv[i], "rb");
        if (f == NULL) {
            printf("%s OPEN_FAILED\n", argv[i]);
            continue;
        }
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
        data = (char*)malloc((size_t)size);
        fread(data, 1, (size_t)size, f);
        fclose(f);

        if (!drmp3_init_memory(&mp3, data, (size_t)size, NULL)) {
            printf("%s INIT_FAILED\n", argv[i]);   /* required: corrupt headers must never fail init */
            free(data);
            continue;
        }

        frames = 0;
        drmp3_get_mp3_and_pcm_frame_count(&mp3, &frames, NULL);
        pcm = drmp3_get_pcm_frame_count(&mp3);
        if (mp3.averageBitrate == 0) {
            /* No usable VBR tag: estimate from file length, mirroring ffprobe's fallback. */
            mp3.averageBitrate = (drmp3_uint32)((drmp3_uint64)size * 8 * mp3.sampleRate / (pcm ? pcm : 1));
        }
        printf("%s frames=%llu duration=%.6f avg_bitrate=%u\n", argv[i],
               (unsigned long long)frames, (double)pcm / (double)mp3.sampleRate,
               mp3.averageBitrate);
        drmp3_uninit(&mp3);
        free(data);
    }
    return 0;
}
