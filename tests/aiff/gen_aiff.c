#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"
static void gen(const char* path, drwav_uint16 tag, drwav_uint16 ch, drwav_uint16 bits, drwav_uint64 frames) {
    drwav_data_format fmt = { drwav_container_aiff, tag, ch, 44100, bits };
    drwav wav; drwav_uint64 i, n = frames * ch;
    drwav_int16 s16[2048]; drwav_uint8 s24[6144]; float f32[2048];
    if (!drwav_init_file_write_aiff(&wav, path, &fmt, NULL)) return;
    for (i = 0; i < n; i++) {
        if (bits == 16) s16[i] = (drwav_int16)(i * 10 - 500);
        else if (bits == 24) { drwav_int32 v = (drwav_int32)i * 1000 - 500000; s24[i*3] = (drwav_uint8)v; s24[i*3+1] = (drwav_uint8)(v >> 8); s24[i*3+2] = (drwav_uint8)(v >> 16); }
        else f32[i] = (float)i * 0.25f - 3.0f; }
    drwav_write_pcm_frames_aiff(&wav, frames, bits == 16 ? (void*)s16 : bits == 24 ? (void*)s24 : (void*)f32);
    drwav_uninit_aiff(&wav);
}
int main(int argc, char** argv) { int t = argc > 1 ? argv[1][0] - '0' : 0;
    if (t == 0) gen("output/mono16.aiff", DR_WAVE_FORMAT_PCM, 1, 16, 512);
    if (t == 1) gen("output/stereo24.aiff", DR_WAVE_FORMAT_PCM, 2, 24, 512);
    if (t == 2) gen("output/stereo32f.aiff", DR_WAVE_FORMAT_IEEE_FLOAT, 2, 32, 512);
    if (t == 3) gen("output/empty.aiff", DR_WAVE_FORMAT_PCM, 1, 16, 0);
    return 0; }
