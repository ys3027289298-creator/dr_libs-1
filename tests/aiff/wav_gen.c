#define DR_WAV_IMPLEMENTATION
#include DRWAV_H
int main(int argc, char** argv) {
    drwav_data_format fmt = { drwav_container_riff, DR_WAVE_FORMAT_PCM, 2, 44100, 16 };
    drwav wav; drwav_int16 s[256]; drwav_uint64 i;
    if (argc < 2) return 1;
    for (i = 0; i < 256; i++) s[i] = (drwav_int16)(i * 7 - 900);
    if (!drwav_init_file_write(&wav, argv[1], &fmt, NULL)) return 1;
    drwav_write_pcm_frames(&wav, 128, s);
    drwav_uninit(&wav);
    return 0;
}
