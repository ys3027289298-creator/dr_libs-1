#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"
#include <stdio.h>

static drmp3_replaygain g_gain;
static int g_have_tag;

static void on_meta(void* user_data, const drmp3_metadata* meta)
{
    (void)user_data;
    if (meta->type == DRMP3_METADATA_TYPE_ID3V2) {
        g_have_tag = 1;
        drmp3_parse_replaygain_id3v2(meta->pRawData, meta->rawDataSize, &g_gain);
    }
}

static void show(const char* label, int has, float value)
{
    if (has) {
        printf("  %-11s = %g\n", label, value);
    } else {
        printf("  %-11s = (empty)\n", label);
    }
}

int main(int argc, char** argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        drmp3 mp3;

        g_have_tag = 0;
        memset(&g_gain, 0, sizeof(g_gain));

        if (!drmp3_init_file_with_metadata(&mp3, argv[i], on_meta, NULL, NULL)) {
            printf("%s: open failed\n", argv[i]);
            continue;
        }

        printf("%s (id3v2 tag: %s)\n", argv[i], g_have_tag ? "yes" : "no");
        show("track gain",  g_gain.hasTrackGain, g_gain.trackGainDB);
        show("track peak",  g_gain.hasTrackPeak, g_gain.trackPeak);
        show("album gain",  g_gain.hasAlbumGain, g_gain.albumGainDB);
        show("album peak",  g_gain.hasAlbumPeak, g_gain.albumPeak);

        drmp3_uninit(&mp3);
    }

    return 0;
}
