#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"
#include <stdio.h>

int main(void)
{
    drmp3_replaygain g;
    unsigned char junk[16] = {0};
    unsigned char hdr[9] = {'I', 'D', '3', 3, 0, 0, 0, 0, 1};
    unsigned char tag[128];
    const char* desc = "REPLAYGAIN_TRACK_GAIN";
    const char* val = "-2.00 dB";
    int n = 0, k, body_start, frame_size, body_size;

    if (drmp3_parse_replaygain_id3v2(NULL, 0, &g) || g.hasTrackGain || g.hasAlbumPeak) return 1;
    if (drmp3_parse_replaygain_id3v2(junk, sizeof(junk), &g)) return 2;
    if (drmp3_parse_replaygain_id3v2(hdr, sizeof(hdr), &g)) return 3;

    tag[n++] = 'I'; tag[n++] = 'D'; tag[n++] = '3';
    tag[n++] = 3;   tag[n++] = 0;   tag[n++] = 0;
    n += 4;  /* Sync-safe tag size, filled in later. */
    tag[n++] = 'T'; tag[n++] = 'X'; tag[n++] = 'X'; tag[n++] = 'X';
    n += 4;  /* Frame size, filled in later. */
    tag[n++] = 0; tag[n++] = 0;
    body_start = n;
    tag[n++] = 1; tag[n++] = 0xFF; tag[n++] = 0xFE;  /* UTF-16LE with BOM. */
    for (k = 0; desc[k]; k++) { tag[n++] = (unsigned char)desc[k]; tag[n++] = 0; }
    tag[n++] = 0; tag[n++] = 0;
    for (k = 0; val[k]; k++)  { tag[n++] = (unsigned char)val[k];  tag[n++] = 0; }
    tag[n++] = 0; tag[n++] = 0;

    frame_size = n - body_start;
    tag[14] = (frame_size >> 24) & 255; tag[15] = (frame_size >> 16) & 255;
    tag[16] = (frame_size >> 8) & 255;  tag[17] = frame_size & 255;
    body_size = frame_size + 10;
    tag[6] = (body_size >> 21) & 0x7F; tag[7] = (body_size >> 14) & 0x7F;
    tag[8] = (body_size >> 7) & 0x7F;  tag[9] = body_size & 0x7F;

    if (!drmp3_parse_replaygain_id3v2(tag, (size_t)n, &g) || !g.hasTrackGain) return 4;
    if (g.trackGainDB < -2.01f || g.trackGainDB > -1.99f) return 5;

    printf("edge cases passed: null/empty/truncated ignored, utf-16 gain=%g\n", g.trackGainDB);
    return 0;
}
