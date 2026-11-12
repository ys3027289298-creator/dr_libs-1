#!/usr/bin/env bash
# Generates the three MP3 test vectors used by the gapless demo.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MEDIA="$ROOT/output/media"
mkdir -p "$MEDIA"
cd "$MEDIA"

# Clean source: a 2.4 s 440 Hz stereo tone. Audio reaches both ends of the file so
# any silence found inside the MP3 is produced solely by the codec's delay/padding.
ffmpeg -hide_banner -loglevel error -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=2.4" \
    -ac 2 -c:a pcm_s16le -y source_tone.wav

# 1) CBR MP3 with an Info/LAME tag (contains encoder delay + padding).
ffmpeg -hide_banner -loglevel error -i source_tone.wav -c:a libmp3lame -b:a 128k \
    -ac 2 -ar 44100 -write_xing 1 -y lame_cbr.mp3

# 2) VBR MP3 with a Xing/LAME tag.
ffmpeg -hide_banner -loglevel error -i source_tone.wav -c:a libmp3lame -q:a 2 \
    -ac 2 -ar 44100 -write_xing 1 -y lame_vbr.mp3

# 3) CBR MP3 with the Xing/Info/LAME header frame stripped: no gapless metadata.
#    Frame structure stays intact, so dr_mp3 must estimate trim from the frames themselves.
ffmpeg -hide_banner -loglevel error -i source_tone.wav -c:a libmp3lame -b:a 128k \
    -ac 2 -ar 44100 -write_xing 1 -y with_xing.mp3
python3 - << 'PY'
data = open('with_xing.mp3', 'rb').read()
def is_sync(d, i): return d[i] == 0xFF and (d[i + 1] & 0xE0) == 0xE0
i = 0
while not is_sync(data, i): i += 1
j = i + 1
while not is_sync(data, j): j += 1
open('nolame_cbr.mp3', 'wb').write(data[j:])
print('stripped header frame bytes %d..%d -> nolame_cbr.mp3 (%d bytes)' % (i, j, len(data) - j))
PY
rm -f with_xing.mp3
ls -l *.mp3
