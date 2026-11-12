/* Reads one generated test file, prints parsed ID3v2 fields and compares them with the expected values. */
#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"
#include <stdio.h>
#include <string.h>

static drmp3_uint8 g_tag[4096]; static size_t g_tagSize; static int g_fails;

static void on_meta(void* u, const drmp3_metadata* m)
{
    (void)u;
    if (m->type == DRMP3_METADATA_TYPE_ID3V2 && m->rawDataSize <= sizeof(g_tag)) {
        memcpy(g_tag, m->pRawData, m->rawDataSize); g_tagSize = m->rawDataSize;
    }
}

static void expect_text(const char* id, const char* expect)
{
    char buf[256] = {0};
    drmp3_bool32 ok = g_tagSize && drmp3_id3v2_read_text(g_tag, g_tagSize, id, buf, sizeof(buf));
    int pass = (expect[0] == '-' && expect[1] == 0) ? !ok : (ok && strcmp(buf, expect) == 0);
    printf("  %-5s = %-16s expect %-16s %s\n", id, ok ? buf : "(none)", expect, pass ? "OK" : "FAIL");
    g_fails += !pass;
}

static void expect_apic(const char* mime, const drmp3_uint8* data, size_t size)
{
    drmp3_id3v2_picture pic;
    drmp3_bool32 ok = g_tagSize && drmp3_id3v2_read_picture(g_tag, g_tagSize, &pic);
    int pass = ok && strcmp(pic.mime, mime) == 0 && pic.size == size && memcmp(pic.pData, data, size) == 0;
    printf("  APIC  = %s, %u bytes %s\n", ok ? pic.mime : "(none)", ok ? (unsigned)pic.size : 0u, pass ? "OK" : "FAIL");
    g_fails += !pass;
}

static const drmp3_uint8 PNG[] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,'d','r','m','p','3','t','e','s','t'};
static const char* NAMES[4]  = {"v23_utf16.mp3", "v24_cover.mp3", "corrupt.mp3", "notag.mp3"};
static const char* EXP[3][4] = {{"歌 Song", "Cover Song", "Survivor", "-"},
                                {"Artist A", "Artist B", "Artist C", "-"},
                                {"Album X", "Album Y", "Album Z", "-"}};

int main(int argc, char** argv)
{
    int i;
    char path[512];
    drmp3 mp3; drmp3_uint64 frames;
    if (argc < 3) return 2;
    i = argv[2][0] - '0';
    if (i < 0 || i > 3) return 2;
    snprintf(path, sizeof(path), "%s/%s", argv[1], NAMES[i]);
    if (!drmp3_init_file_with_metadata(&mp3, path, on_meta, NULL, NULL)) { printf("  init FAIL\n"); return 1; }
    frames = drmp3_get_pcm_frame_count(&mp3);
    printf("[%s] decode ok, pcm frames = %llu\n", NAMES[i], (unsigned long long)frames);
    if (frames == 0) { printf("  decode FAIL\n"); g_fails++; }
    expect_text("TIT2", EXP[0][i]); expect_text("TPE1", EXP[1][i]); expect_text("TALB", EXP[2][i]);
    if (i == 1) expect_apic("image/png", PNG, sizeof(PNG));
    drmp3_uninit(&mp3);
    printf("%s: %s\n", NAMES[i], g_fails ? "FAIL" : "ALL OK");
    return g_fails != 0;
}
