/* Minimal MPEG-1 Layer I frame decoder driver for dr_mp3 conformance checks. */
#define DR_MP3_IMPLEMENTATION
#include "../dr_mp3.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    FILE* f;
    long size, offset = 0, frames = 0, bad = 0;
    unsigned char* data;
    drmp3dec dec;
    short pcm[384 * 2];
    short first[8] = {0};
    drmp3dec_frame_info info;
    int rate = 0, channels = 0, i, samples;

    (void)argc;
    f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 2; }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char*)malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) { return 2; }
    fclose(f);

    drmp3dec_init(&dec);
    while (offset < size) {
        samples = drmp3dec_decode_frame(&dec, data + offset, (int)(size - offset), pcm, &info);
        if (info.frame_bytes <= 0) break;
        if (samples == 0) { bad++; offset += info.frame_bytes; continue; }
        if (frames == 0) {
            rate = info.sample_rate;
            channels = info.channels;
            for (i = 0; i < 8; i++) first[i] = pcm[i];
        }
        frames++;
        offset += info.frame_bytes;
    }

    printf("sample_rate=%d channels=%d frames=%ld bad_frames=%ld\n", rate, channels, frames, bad);
    printf("first8:");
    for (i = 0; i < 8; i++) printf(" %d", first[i]);
    printf("\n");
    return 0;
}
