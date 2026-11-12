/* Writes one WAV per case with drwav_init_file_write_with_loops(), reads it back and reports the smpl loop points. */
#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"
#include <stdio.h>
#include <string.h>

#define FRAME_COUNT 2048

static void write_pcm(drwav* pWav)
{
    drwav_int16 samples[FRAME_COUNT];
    drwav_uint32 i;
    for (i = 0; i < FRAME_COUNT; ++i) {
        samples[i] = (drwav_int16)(i * 16 - 16000);
    }
    drwav_write_pcm_frames(pWav, FRAME_COUNT, samples);
}

static int readback(const char* name, const char* path, drwav_uint32 expectCount, const drwav_loop_point* pExpect)
{
    drwav wav;
    drwav_uint32 m, i, count = 0;
    drwav_smpl_loop* pLoops = NULL;
    int match = 1;
    if (!drwav_init_file_with_metadata(&wav, path, DRWAV_WITH_METADATA, NULL)) {
        printf("case=%s readback=failed\n", name);
        return 1;
    }
    for (m = 0; m < wav.metadataCount; ++m) {
        if (wav.pMetadata[m].type == drwav_metadata_type_smpl) {
            count  = wav.pMetadata[m].data.smpl.sampleLoopCount;
            pLoops = wav.pMetadata[m].data.smpl.pLoops;
        }
    }
    if (count != expectCount) { match = 0; }
    for (i = 0; i < count; ++i) {
        printf("case=%s loop=%u start=%u end=%u\n", name, i, pLoops[i].firstSampleOffset, pLoops[i].lastSampleOffset);
        if (i >= expectCount || pLoops[i].firstSampleOffset != pExpect[i].firstSampleOffset
                              || pLoops[i].lastSampleOffset  != pExpect[i].lastSampleOffset) {
            match = 0;
        }
    }
    printf("case=%s loops=%u match=%s\n", name, count, match ? "yes" : "no");
    drwav_uninit(&wav);
    return match ? 0 : 1;
}

int main(int argc, char** argv)
{
    drwav_data_format format;
    drwav_loop_point loops[DRWAV_MAX_SMPL_LOOPS + 1];
    drwav_uint32 i, count = 0;
    drwav wav;
    drwav_bool32 ok;
    if (argc < 3) { return 2; }

    memset(&format, 0, sizeof(format));
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 1;
    format.sampleRate = 44100;
    format.bitsPerSample = 16;

    if      (strcmp(argv[1], "single")   == 0) { count = 1; }
    else if (strcmp(argv[1], "dual")     == 0) { count = 2; }
    else if (strcmp(argv[1], "overflow") == 0) { count = DRWAV_MAX_SMPL_LOOPS + 1; }
    else if (strcmp(argv[1], "none")     == 0) { count = 0; }
    else if (strcmp(argv[1], "baseline") == 0) {
        ok = drwav_init_file_write(&wav, argv[2], &format, NULL);
        if (!ok) { return 1; }
        write_pcm(&wav);
        drwav_uninit(&wav);
        printf("case=baseline written\n");
        return 0;
    }
    else { return 2; }

    for (i = 0; i < count; ++i) {
        loops[i].firstSampleOffset = 100 + i * 500;
        loops[i].lastSampleOffset  = 400 + i * 500;
        loops[i].type              = drwav_smpl_loop_type_forward;
    }

    ok = drwav_init_file_write_with_loops(&wav, argv[2], &format, count ? loops : NULL, count, NULL);
    if (!ok) {
        printf("case=%s write=rejected\n", argv[1]);
        return (count > DRWAV_MAX_SMPL_LOOPS) ? 0 : 1;    /* Rejection is the expected outcome for overflow. */
    }
    write_pcm(&wav);
    drwav_uninit(&wav);
    return readback(argv[1], argv[2], count, loops);
}
