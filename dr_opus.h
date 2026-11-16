/*
Opus (Ogg) audio decoder. David Reid - mackron@gmail.com
GitHub: https://github.com/mackron/dr_libs

This is a single-file library. To use it, do something like the following in one .c file.

    #define DR_OPUS_IMPLEMENTATION
    #include "dr_opus.h"

You can then #include this file in other parts of the program as you would with any other header.

This library decodes Opus streams from an Ogg container and outputs PCM in the same style as
dr_mp3, dr_wav and dr_flac: initialize, then pull PCM frames with dropus_read_pcm_frames_*().

ARCHITECTURE NOTE
=================
The Ogg container layer, the Opus packet (TOC) parsing, packet-loss concealment scheduling and
the PCM output path are implemented in this file. The Opus packet decoder itself (SILK/CELT) is
NOT reimplemented here. Instead, it is loaded at runtime from the operating system's libopus
shared library (libopus.so.0, libopus.dylib or libopus.dll) via dlopen()/LoadLibrary(). This
keeps dr_libs' zero-build-dependency promise: there is nothing to install at build time and
nothing to link against. If libopus cannot be found at runtime, initialization fails gracefully
and dropus_init_*() returns false.

Corrupt or missing Ogg pages/packets do not abort decoding. The Ogg layer resynchronizes to the
next valid page, lost audio is concealed with libopus PLC (packet-loss concealment) so the output
timeline stays sample-aligned with the undamaged stream, and decoding continues. The number of
concealed PCM frames and lost packets is reported in the dropus struct.

LIMITATIONS
===========
  * Channel mapping family 0 only (mono and stereo). Other families fail at initialization.
  * No seeking.
  * Chained Ogg streams are not supported; the first Opus logical stream is decoded.


QUICK NOTES
===========
- Decoded output is always 48 kHz. The channel count is taken from the OpusHead header.
- Samples are decoded internally as float and converted to s16 on demand.


OPTIONS
=======
#define these options before including this file.

#define DR_OPUS_NO_STDIO
  Disable dropus_init_file() and family.

#define DR_OPUS_LIBOPUS_PATH "path-or-soname"
  Override the runtime libopus library name. Only the specified name is tried.
*/

#ifndef dr_opus_h
#define dr_opus_h

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DROPUS_VERSION_MAJOR     0
#define DROPUS_VERSION_MINOR     1
#define DROPUS_VERSION_REVISION  0
#define DROPUS_XSTRINGIFY(x)     #x
#define DROPUS_STRINGIFY(x)      DROPUS_XSTRINGIFY(x)
#define DROPUS_VERSION_STRING    DROPUS_STRINGIFY(DROPUS_VERSION_MAJOR) "." DROPUS_STRINGIFY(DROPUS_VERSION_MINOR) "." DROPUS_STRINGIFY(DROPUS_VERSION_REVISION)

#ifdef _MSC_VER
    #if _MSC_VER < 1600
    typedef   signed char    dropus_int8;
    typedef unsigned char    dropus_uint8;
    typedef   signed short   dropus_int16;
    typedef unsigned short   dropus_uint16;
    typedef   signed int     dropus_int32;
    typedef unsigned int     dropus_uint32;
    typedef   signed __int64 dropus_int64;
    typedef unsigned __int64 dropus_uint64;
    #else
    #include <stdint.h>
    typedef int8_t           dropus_int8;
    typedef uint8_t          dropus_uint8;
    typedef int16_t          dropus_int16;
    typedef uint16_t         dropus_uint16;
    typedef int32_t          dropus_int32;
    typedef uint32_t         dropus_uint32;
    typedef int64_t          dropus_int64;
    typedef uint64_t         dropus_uint64;
    #endif
#else
    #include <stdint.h>
    typedef int8_t           dropus_int8;
    typedef uint8_t          dropus_uint8;
    typedef int16_t          dropus_int16;
    typedef uint16_t         dropus_uint16;
    typedef int32_t          dropus_int32;
    typedef uint32_t         dropus_uint32;
    typedef int64_t          dropus_int64;
    typedef uint64_t         dropus_uint64;
#endif

typedef int                  dropus_bool32;
#define DROPUS_TRUE          1
#define DROPUS_FALSE         0

#if defined(_MSC_VER)
    #define DROPUS_API extern
#else
    #define DROPUS_API
#endif
#define DROPUS_PRIVATE static

/* Sized to the format limits so memory usage is bounded regardless of input. */
#define DROPUS_MAX_PAGE_HEADER_BYTES   (27 + 255)          /* Ogg page header + segment table. */
#define DROPUS_MAX_PAGE_DATA_BYTES     (255*255)           /* 65025: max payload of one Ogg page. */
#define DROPUS_MAX_PACKET_BYTES        (1275*48)           /* 61200: RFC 6716 max Opus packet (48 frames x 1275 bytes). */
#define DROPUS_MAX_FRAME_SAMPLES       5760                /* 120ms at 48kHz: max decoded frames per packet. */
#define DROPUS_MAX_CHANNELS            2                   /* Channel mapping family 0. */
#define DROPUS_SAMPLE_RATE             48000               /* Opus always decodes at 48kHz. */


/* Allocation Callbacks */
typedef struct
{
    void* pUserData;
    void* (* onMalloc)(size_t sz, void* pUserData);
    void* (* onRealloc)(void* p, size_t sz, void* pUserData);
    void  (* onFree)(void* p, void* pUserData);
} dropus_allocation_callbacks;
/* End Allocation Callbacks */


/*
Callback for when data is read. Return value is the number of bytes actually read.

A return value of less than bytesToRead indicates the end of the stream.
*/
typedef size_t (* dropus_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);


typedef struct
{
    /* Public stream information. Set on initialization. */
    dropus_uint32 channels;                 /* 1 or 2, from the OpusHead header. */
    dropus_uint32 sampleRate;               /* Always 48000. */
    dropus_uint32 preSkipInPCMFrames;       /* Encoder delay, discarded from the start of the output. */
    dropus_int32  outputGain;               /* Raw OpusHead output gain. Applied to decoded samples. */

    /* Counters for the packet-loss path. Updated as decoding progresses. */
    dropus_uint64 totalPCMFramesConcealed;  /* PCM frames generated by packet-loss concealment. */
    dropus_uint64 totalPacketsLost;         /* Packets dropped or replaced by concealment. */
    dropus_uint64 totalPagesResynced;       /* Ogg pages skipped due to bad capture pattern or CRC. */

    /* Internal state below. Do not rely on these. */
    dropus_read_proc onRead;
    void* pUserData;
    dropus_allocation_callbacks allocationCallbacks;
    void* pOpusDecoder;                     /* Opaque libopus decoder state, heap allocated by libopus. */
    float outputGainLinear;
    dropus_uint32 oggSerial;
    dropus_uint32 oggNextSequence;
    dropus_bool32 oggHasSequence;
    dropus_bool32 oggAtContinuedPacket;     /* The current page continues a packet from the previous page. */
    dropus_bool32 oggPageEOS;
    dropus_bool32 oggPageGranuleValid;
    dropus_int64  oggPageGranule;
    dropus_uint32 oggPageSegmentCount;
    dropus_uint32 oggPageSegmentIndex;      /* Index of the next segment to consume. */
    dropus_uint32 oggPageDataOffset;        /* Offset into pageData of the next segment's data. */
    dropus_uint32 oggPageDataSize;
    dropus_bool32 oggDroppingContinued;     /* Dropping the remainder of a packet whose start was lost. */
    dropus_uint8  oggPageHeader[DROPUS_MAX_PAGE_HEADER_BYTES];
    dropus_uint8  oggPageData[DROPUS_MAX_PAGE_DATA_BYTES];
    dropus_uint8  packet[DROPUS_MAX_PACKET_BYTES];
    dropus_uint32 packetSize;
    dropus_uint32 packetFill;               /* Bytes assembled so far for the current packet. */
    dropus_int64  packetGranule;            /* Granule position of the page the current packet completed on. */
    dropus_bool32 packetGranuleValid;
    dropus_uint64 decodedTotal;             /* Position in the granule domain (includes pre-skip and concealed samples). */
    dropus_uint64 preSkipRemaining;
    dropus_uint64 pendingConceal;           /* Samples still to be generated by PLC before the next real packet. */
    dropus_uint32 pcmFramesRemaining;
    dropus_uint32 pcmFramesConsumed;
    dropus_bool32 atEnd;
    int           headerStage;              /* 0 = expect OpusHead, 1 = expect OpusTags, 2 = audio. */
    float pcm[DROPUS_MAX_FRAME_SAMPLES * DROPUS_MAX_CHANNELS];
#ifndef DR_OPUS_NO_STDIO
    void* pFile;                            /* FILE*. Only used by dropus_init_file(). */
#endif
    struct
    {
        const dropus_uint8* pData;
        size_t dataSize;
        size_t currentReadPos;
    } memory;                               /* Only used by dropus_init_memory(). */
} dropus;


DROPUS_API void dropus_version(dropus_uint32* pMajor, dropus_uint32* pMinor, dropus_uint32* pRevision);
DROPUS_API const char* dropus_version_string(void);

/*
Initializes an Opus decoder from a generic read callback.

The decoder configures itself from the stream: the sample rate is always 48000 and the channel
count is taken from the OpusHead header. Returns false if the stream is not a valid Ogg Opus
stream, if the stream uses an unsupported channel mapping family, or if libopus could not be
loaded at runtime.
*/
DROPUS_API dropus_bool32 dropus_init(dropus* pOpus, dropus_read_proc onRead, void* pUserData, const dropus_allocation_callbacks* pAllocationCallbacks);

#ifndef DR_OPUS_NO_STDIO
/* Initializes an Opus decoder from a file. */
DROPUS_API dropus_bool32 dropus_init_file(dropus* pOpus, const char* pFilePath, const dropus_allocation_callbacks* pAllocationCallbacks);
#endif

/*
Initializes an Opus decoder from a block of memory.

This does not create a copy of the data. The buffer must remain valid for the lifetime of the
dropus object.
*/
DROPUS_API dropus_bool32 dropus_init_memory(dropus* pOpus, const void* pData, size_t dataSize, const dropus_allocation_callbacks* pAllocationCallbacks);

/* Uninitializes an Opus decoder. */
DROPUS_API void dropus_uninit(dropus* pOpus);

/*
Reads PCM frames as interleaved 32-bit floats. Returns the number of PCM frames actually read,
which is less than framesToRead when the end of the stream is reached.
*/
DROPUS_API dropus_uint64 dropus_read_pcm_frames_f32(dropus* pOpus, dropus_uint64 framesToRead, float* pBufferOut);

/* Reads PCM frames as interleaved signed 16-bit integers. */
DROPUS_API dropus_uint64 dropus_read_pcm_frames_s16(dropus* pOpus, dropus_uint64 framesToRead, dropus_int16* pBufferOut);

/* Reports the size in bytes of the libopus decoder state for the given channel count, or 0 if unknown. For diagnostics only. */
DROPUS_API dropus_uint32 dropus_opus_decoder_state_size(int channels);


#ifdef __cplusplus
}
#endif
#endif  /* dr_opus_h */


/************************************************************************************************************************************************************
 ***********************************************************************************************************************************************************

 IMPLEMENTATION

 ***********************************************************************************************************************************************************
 **********************************************************************************************************************************************************/
#if defined(DR_OPUS_IMPLEMENTATION) || defined(DR_OPUS_IMPLEMENTATION_STATIC)
#ifndef dr_opus_c
#define dr_opus_c

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef DR_OPUS_NO_STDIO
#include <stdio.h>
#endif

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif


/************************************************************************************************************************************************************

 libopus runtime loading

 The Opus packet decoder is loaded from the system libopus at runtime. No libopus headers or
 import libraries are required at build time.

*********************************************************************************************/
typedef void*       (* dropus_opus_decoder_create_proc)(dropus_int32 sampleRate, int channels, int* pError);
typedef void        (* dropus_opus_decoder_destroy_proc)(void* pDecoder);
typedef int         (* dropus_opus_decode_float_proc)(void* pDecoder, const unsigned char* pData, dropus_int32 len, float* pPCM, int frameSize, int decodeFEC);
typedef int         (* dropus_opus_decoder_get_size_proc)(int channels);

typedef struct
{
    int state;      /* 0 = not attempted, 1 = loaded, -1 = failed. */
#if defined(_WIN32)
    HMODULE hModule;
#else
    void* pHandle;
#endif
    dropus_opus_decoder_create_proc decoder_create;
    dropus_opus_decoder_destroy_proc decoder_destroy;
    dropus_opus_decode_float_proc decode_float;
    dropus_opus_decoder_get_size_proc decoder_get_size;     /* Optional. Used only for diagnostics. */
} dropus_opus_api;

static dropus_opus_api g_dropusOpus;

/*
Loads libopus. This is not thread-safe on first call, matching the general dr_libs convention
that initialization of independent objects from multiple threads requires external synchronization
only when shared global state is involved. Call dropus_init_*() once from a single thread before
decoding concurrently, or accept that concurrent first-time initialization may race.
*/
static dropus_bool32 dropus__load_opus(void)
{
    static const char* pDefaultNames[] = {
#if defined(_WIN32)
        "libopus.dll",
        "opus.dll"
#elif defined(__APPLE__)
        "libopus.0.dylib",
        "libopus.dylib"
#else
        "libopus.so.0",
        "libopus.so"
#endif
    };
    size_t i;

    if (g_dropusOpus.state != 0) {
        return (g_dropusOpus.state > 0) ? DROPUS_TRUE : DROPUS_FALSE;
    }

    g_dropusOpus.state = -1;    /* Assume failure. Flipped to 1 on success below. */

#ifdef DR_OPUS_LIBOPUS_PATH
    {
        const char* pPath = DR_OPUS_LIBOPUS_PATH;
#if defined(_WIN32)
        g_dropusOpus.hModule = LoadLibraryA(pPath);
#else
        g_dropusOpus.pHandle = dlopen(pPath, RTLD_LAZY | RTLD_LOCAL);
#endif
    }
#else
    for (i = 0; i < sizeof(pDefaultNames)/sizeof(pDefaultNames[0]); ++i) {
#if defined(_WIN32)
        g_dropusOpus.hModule = LoadLibraryA(pDefaultNames[i]);
        if (g_dropusOpus.hModule != NULL) {
            break;
        }
#else
        g_dropusOpus.pHandle = dlopen(pDefaultNames[i], RTLD_LAZY | RTLD_LOCAL);
        if (g_dropusOpus.pHandle != NULL) {
            break;
        }
#endif
    }
#endif

#if defined(_WIN32)
    if (g_dropusOpus.hModule == NULL) {
        return DROPUS_FALSE;
    }
    /* memcpy through a void* to avoid the ISO C object-pointer-to-function-pointer cast. */
    #define DROPUS__GET_OPUS_PROC(name) \
        do { \
            void* pProc = (void*)GetProcAddress(g_dropusOpus.hModule, "opus_" #name); \
            memcpy(&g_dropusOpus.name, &pProc, sizeof(pProc)); \
        } while (0)
#else
    if (g_dropusOpus.pHandle == NULL) {
        return DROPUS_FALSE;
    }
    #define DROPUS__GET_OPUS_PROC(name) \
        do { \
            void* pProc = dlsym(g_dropusOpus.pHandle, "opus_" #name); \
            memcpy(&g_dropusOpus.name, &pProc, sizeof(pProc)); \
        } while (0)
#endif

    DROPUS__GET_OPUS_PROC(decoder_create);
    DROPUS__GET_OPUS_PROC(decoder_destroy);
    DROPUS__GET_OPUS_PROC(decode_float);
    DROPUS__GET_OPUS_PROC(decoder_get_size);    /* Optional. */

    #undef DROPUS__GET_OPUS_PROC

    if (g_dropusOpus.decoder_create == NULL || g_dropusOpus.decoder_destroy == NULL || g_dropusOpus.decode_float == NULL) {
        return DROPUS_FALSE;
    }

    g_dropusOpus.state = 1;
    return DROPUS_TRUE;
}

/* Reports the size in bytes of the libopus decoder state for the given channel count, or 0 if unknown. For diagnostics only. */
DROPUS_API dropus_uint32 dropus_opus_decoder_state_size(int channels)
{
    if (!dropus__load_opus() || g_dropusOpus.decoder_get_size == NULL) {
        return 0;
    }
    return (dropus_uint32)g_dropusOpus.decoder_get_size(channels);
}


/************************************************************************************************************************************************************

 Utilities

*********************************************************************************************/
static dropus_uint16 dropus__read_u16le(const dropus_uint8* p)
{
    return (dropus_uint16)((dropus_uint16)p[0] | ((dropus_uint16)p[1] << 8));
}

static dropus_uint32 dropus__read_u32le(const dropus_uint8* p)
{
    return (dropus_uint32)p[0] | ((dropus_uint32)p[1] << 8) | ((dropus_uint32)p[2] << 16) | ((dropus_uint32)p[3] << 24);
}

static dropus_uint64 dropus__read_u64le(const dropus_uint8* p)
{
    return (dropus_uint64)dropus__read_u32le(p) | ((dropus_uint64)dropus__read_u32le(p + 4) << 32);
}


/* Ogg CRC32: polynomial 0x04C11DB7, no reflection, init 0, no final XOR. Pass 0 as crc for a new checksum. */
static dropus_uint32 dropus__ogg_crc32(dropus_uint32 crc, const dropus_uint8* pData, size_t dataSize)
{
    static dropus_uint32 table[256];
    static dropus_bool32 tableInitialized = DROPUS_FALSE;
    size_t i;

    if (!tableInitialized) {
        for (i = 0; i < 256; ++i) {
            int bit;
            dropus_uint32 r = (dropus_uint32)i << 24;
            for (bit = 0; bit < 8; ++bit) {
                r = (r << 1) ^ ((r & 0x80000000u) ? 0x04C11DB7u : 0);
            }
            table[i] = r;
        }
        tableInitialized = DROPUS_TRUE;
    }

    for (i = 0; i < dataSize; ++i) {
        crc = (crc << 8) ^ table[((crc >> 24) ^ pData[i]) & 0xFF];
    }

    return crc;
}


static dropus_bool32 dropus__read_bytes(dropus* pOpus, void* pOut, size_t bytesToRead)
{
    size_t bytesRead = pOpus->onRead(pOpus->pUserData, pOut, bytesToRead);
    return (bytesRead == bytesToRead) ? DROPUS_TRUE : DROPUS_FALSE;
}


/************************************************************************************************************************************************************

 Ogg layer

 Reads pages, verifies capture pattern and CRC, resynchronizes on corruption and assembles
 packets from segments. Packet loss is surfaced to the decoder layer which schedules PLC.

*********************************************************************************************/
typedef enum
{
    dropus_page_result_ok,
    dropus_page_result_eof,
    dropus_page_result_error
} dropus_page_result;

/*
Reads the next valid page belonging to the Opus stream. Pages with a bad capture pattern or CRC
are skipped (resync), as are pages from other logical streams. Sets oggGapDetected if one or
more pages were lost or skipped since the previous valid page.
*/
static dropus_page_result dropus__read_page(dropus* pOpus, dropus_bool32* pGapDetected)
{
    *pGapDetected = DROPUS_FALSE;

    for (;;) {
        dropus_uint8* pHeader = pOpus->oggPageHeader;
        dropus_uint32 pageDataSize;
        dropus_uint32 segmentCount;
        dropus_uint32 i;
        dropus_uint32 crcExpected;
        dropus_uint32 crcActual;
        dropus_uint32 sequence;
        dropus_uint32 serial;

        /* Capture pattern, with byte-wise resync on corruption. */
        if (!dropus__read_bytes(pOpus, pHeader, 4)) {
            return dropus_page_result_eof;
        }
        if (pHeader[0] != 'O' || pHeader[1] != 'g' || pHeader[2] != 'g' || pHeader[3] != 'S') {
            pOpus->totalPagesResynced += 1;
            *pGapDetected = DROPUS_TRUE;
            while (pHeader[0] != 'O' || pHeader[1] != 'g' || pHeader[2] != 'g' || pHeader[3] != 'S') {
                pHeader[0] = pHeader[1];
                pHeader[1] = pHeader[2];
                pHeader[2] = pHeader[3];
                if (!dropus__read_bytes(pOpus, pHeader + 3, 1)) {
                    return dropus_page_result_eof;
                }
            }
        }

        if (!dropus__read_bytes(pOpus, pHeader + 4, 23)) {
            return dropus_page_result_eof;
        }

        if (pHeader[4] != 0) {
            continue;   /* Unknown Ogg version. Skip: resync by continuing the loop (capture scan restarts). */
        }

        segmentCount = pHeader[26];
        if (!dropus__read_bytes(pOpus, pHeader + 27, segmentCount)) {
            return dropus_page_result_eof;
        }

        pageDataSize = 0;
        for (i = 0; i < segmentCount; ++i) {
            pageDataSize += pHeader[27 + i];
        }

        if (!dropus__read_bytes(pOpus, pOpus->oggPageData, pageDataSize)) {
            return dropus_page_result_eof;
        }

        /* CRC is computed with the CRC field zeroed. */
        crcExpected = dropus__read_u32le(pHeader + 22);
        pHeader[22] = 0; pHeader[23] = 0; pHeader[24] = 0; pHeader[25] = 0;
        crcActual = dropus__ogg_crc32(0, pHeader, 27 + segmentCount);
        crcActual = dropus__ogg_crc32(crcActual, pOpus->oggPageData, pageDataSize);

        if (crcActual != crcExpected) {
            pOpus->totalPagesResynced += 1;
            *pGapDetected = DROPUS_TRUE;
            continue;   /* Corrupt page: drop it and resync from the next capture pattern. */
        }

        serial = dropus__read_u32le(pHeader + 14);
        if (!pOpus->oggHasSequence) {
            pOpus->oggSerial = serial;
            pOpus->oggNextSequence = dropus__read_u32le(pHeader + 18);
            pOpus->oggHasSequence = DROPUS_TRUE;
        }

        if (serial != pOpus->oggSerial) {
            continue;   /* A different logical stream (grouped/chained). Ignore it. */
        }

        sequence = dropus__read_u32le(pHeader + 18);
        if (sequence != pOpus->oggNextSequence) {
            *pGapDetected = DROPUS_TRUE;    /* One or more whole pages were lost. */
        }
        pOpus->oggNextSequence = sequence + 1;

        pOpus->oggAtContinuedPacket = (pHeader[5] & 0x01) ? DROPUS_TRUE : DROPUS_FALSE;
        pOpus->oggPageEOS           = (pHeader[5] & 0x04) ? DROPUS_TRUE : DROPUS_FALSE;
        {
            dropus_uint64 granule = dropus__read_u64le(pHeader + 6);
            if (granule == ~(dropus_uint64)0) {
                pOpus->oggPageGranuleValid = DROPUS_FALSE;
                pOpus->oggPageGranule = 0;
            } else {
                pOpus->oggPageGranuleValid = DROPUS_TRUE;
                pOpus->oggPageGranule = (dropus_int64)granule;
            }
        }
        pOpus->oggPageSegmentCount = segmentCount;
        pOpus->oggPageSegmentIndex = 0;
        pOpus->oggPageDataOffset = 0;
        pOpus->oggPageDataSize = pageDataSize;

        return dropus_page_result_ok;
    }
}


/*
Returns the duration in 48kHz samples of a complete Opus packet, or 0 if the packet header is
invalid. Parsed from the TOC byte per RFC 6716 section 3.1. Does not require libopus.
*/
static dropus_uint32 dropus__packet_samples(const dropus_uint8* pData, dropus_uint32 dataSize)
{
    dropus_uint32 toc;
    dropus_uint32 config;
    dropus_uint32 frameSize;
    dropus_uint32 frameCount;
    dropus_uint32 total;

    if (dataSize < 1) {
        return 0;
    }

    toc = pData[0];
    config = (toc >> 3) & 0x1F;

    if (config < 12) {
        frameSize = 480 << (config & 3);        /* SILK: 10, 20, 40, 60 ms. */
    } else if (config < 16) {
        frameSize = 480 << (config & 1);        /* Hybrid: 10, 20 ms. */
    } else {
        frameSize = 120 << (config & 3);        /* CELT: 2.5, 5, 10, 20 ms. */
    }

    switch (toc & 3) {
        case 0: frameCount = 1; break;
        case 1: frameCount = 2; break;          /* Two equal-size compressed frames. */
        case 2: frameCount = 2; break;          /* Two equal-size frames. */
        default:
        {
            if (dataSize < 2) {
                return 0;
            }
            frameCount = pData[1] & 0x3F;
            if (frameCount == 0) {
                return 0;
            }
        } break;
    }

    total = frameSize * frameCount;
    if (total > DROPUS_MAX_FRAME_SAMPLES) {
        return 0;   /* Exceeds the 120ms maximum. Invalid. */
    }

    return total;
}


/*
Sums the durations of all complete packets terminating on the current page. Used to compute how
many samples were lost across a page gap. Packets continuing onto the next page are excluded,
matching the granule-position semantics (a page's granule counts packets completing on it).
*/
static dropus_uint64 dropus__page_complete_samples(dropus* pOpus)
{
    dropus_uint64 total = 0;
    dropus_uint32 offset = 0;
    dropus_uint32 packetStart = 0;
    dropus_uint32 i;
    dropus_bool32 skipFirst = (pOpus->oggAtContinuedPacket && pOpus->oggDroppingContinued) ? DROPUS_TRUE : DROPUS_FALSE;
    dropus_bool32 skipping = skipFirst;

    for (i = 0; i < pOpus->oggPageSegmentCount; ++i) {
        dropus_uint32 segmentSize = pOpus->oggPageHeader[27 + i];
        if (segmentSize < 255) {
            /* A packet terminates here. */
            if (!skipping) {
                dropus_uint32 samples = dropus__packet_samples(pOpus->oggPageData + packetStart, (offset + segmentSize) - packetStart);
                total += samples;
            }
            skipping = DROPUS_FALSE;
            packetStart = offset + segmentSize;
        }
        offset += segmentSize;
    }

    return total;
}


/*
Assembles the next complete Opus packet. Returns DROPUS_FALSE at the end of the stream. On a
page gap, schedules concealment for the lost samples (pOpus->pendingConceal) before returning
the first packet after the gap.
*/
static dropus_bool32 dropus__read_next_packet(dropus* pOpus)
{
    for (;;) {
        /* Load a new page if the current one is exhausted. */
        if (pOpus->oggPageSegmentIndex >= pOpus->oggPageSegmentCount) {
            dropus_bool32 gapDetected;
            dropus_page_result pageResult;

            if (pOpus->oggPageEOS && pOpus->oggHasSequence) {
                return DROPUS_FALSE;    /* Clean end of stream. */
            }

            pageResult = dropus__read_page(pOpus, &gapDetected);
            if (pageResult != dropus_page_result_ok) {
                return DROPUS_FALSE;
            }

            /* A partial packet from the previous page cannot be completed across a gap. */
            if (pOpus->packetFill > 0 && (!pOpus->oggAtContinuedPacket || gapDetected)) {
                pOpus->packetFill = 0;
                pOpus->totalPacketsLost += 1;
                if (pOpus->pendingConceal == 0) {
                    pOpus->pendingConceal = 960;    /* Unknown duration: conceal one 20ms frame. */
                }
            }

            /* If the page starts with a continued packet but we have nothing buffered (its start
            was lost, or this is the first page), drop the continuation up to the first packet
            boundary. */
            pOpus->oggDroppingContinued = DROPUS_FALSE;
            if (pOpus->oggAtContinuedPacket && pOpus->packetFill == 0) {
                pOpus->oggDroppingContinued = DROPUS_TRUE;
            }

            if (gapDetected && pOpus->headerStage == 2) {
                /* Work out how many samples were lost and schedule concealment. */
                if (pOpus->oggPageGranuleValid) {
                    dropus_uint64 pageSamples = dropus__page_complete_samples(pOpus);
                    dropus_int64 pageStart = (dropus_int64)(pOpus->oggPageGranule - pageSamples);
                    dropus_int64 lost = pageStart - (dropus_int64)pOpus->decodedTotal;
                    if (lost > 0) {
                        dropus_uint64 conceal = (dropus_uint64)lost;
                        dropus_uint64 maxConceal = (dropus_uint64)DROPUS_SAMPLE_RATE * 60;  /* Cap absurd gaps at 60s. */
                        if (conceal > maxConceal) {
                            conceal = maxConceal;
                        }
                        pOpus->pendingConceal += conceal;
                        pOpus->totalPacketsLost += (conceal + 959) / 960;   /* Approximate packet count for reporting. */
                    }
                } else {
                    pOpus->pendingConceal += 960;
                    pOpus->totalPacketsLost += 1;
                }
            }
        }

        /* Consume segments until a packet is complete. */
        while (pOpus->oggPageSegmentIndex < pOpus->oggPageSegmentCount) {
            dropus_uint32 segmentIndex = pOpus->oggPageSegmentIndex;
            dropus_uint32 segmentSize = pOpus->oggPageHeader[27 + segmentIndex];
            const dropus_uint8* pSegmentData = pOpus->oggPageData + pOpus->oggPageDataOffset;

            pOpus->oggPageSegmentIndex += 1;
            pOpus->oggPageDataOffset += segmentSize;

            if (pOpus->oggDroppingContinued) {
                if (segmentSize < 255) {
                    pOpus->oggDroppingContinued = DROPUS_FALSE;     /* Dropped up to the next packet boundary. */
                }
                continue;
            }

            if (pOpus->packetFill + segmentSize > DROPUS_MAX_PACKET_BYTES) {
                /* Packet exceeds the format maximum: the stream is corrupt. Drop it and conceal. */
                pOpus->packetFill = 0;
                pOpus->totalPacketsLost += 1;
                pOpus->pendingConceal += 960;
                if (segmentSize < 255) {
                    continue;
                }
                continue;
            }

            memcpy(pOpus->packet + pOpus->packetFill, pSegmentData, segmentSize);
            pOpus->packetFill += segmentSize;

            if (segmentSize < 255) {
                /* Packet complete. */
                pOpus->packetSize = pOpus->packetFill;
                pOpus->packetFill = 0;
                pOpus->packetGranuleValid = pOpus->oggPageGranuleValid;
                pOpus->packetGranule = pOpus->oggPageGranule;
                return DROPUS_TRUE;
            }
        }
    }
}


/************************************************************************************************************************************************************

 Decoder layer

*********************************************************************************************/
static void dropus__apply_gain(dropus* pOpus, float* pPCM, dropus_uint32 frameCount)
{
    if (pOpus->outputGainLinear != 1.0f) {
        dropus_uint32 sampleCount = frameCount * pOpus->channels;
        dropus_uint32 i;
        for (i = 0; i < sampleCount; ++i) {
            pPCM[i] *= pOpus->outputGainLinear;
        }
    }
}

/*
Fills the internal PCM buffer with the next chunk of decoded or concealed audio. Returns the
number of PCM frames now available, 0 at the end of the stream.
*/
static dropus_uint32 dropus__fill_pcm(dropus* pOpus)
{
    dropus_uint32 framesAvailable;

    if (pOpus->atEnd) {
        return 0;
    }

    for (;;) {
        dropus_uint32 frames;
        dropus_bool32 isConceal = DROPUS_FALSE;

        /* Conceal lost audio first so the timeline stays aligned with the undamaged stream. */
        if (pOpus->pendingConceal > 0) {
            static const dropus_uint32 pValidSizes[] = { 5760, 2880, 1920, 960, 480, 240, 120 };
            dropus_uint32 chunk = 120;
            size_t i;
            int result;

            for (i = 0; i < sizeof(pValidSizes)/sizeof(pValidSizes[0]); ++i) {
                if (pValidSizes[i] <= pOpus->pendingConceal) {
                    chunk = pValidSizes[i];
                    break;
                }
            }
            if (pOpus->pendingConceal < 120) {
                /* Residual that is not a valid Opus frame size: drop it to stay bounded. */
                pOpus->pendingConceal = 0;
                continue;
            }

            result = g_dropusOpus.decode_float(pOpus->pOpusDecoder, NULL, 0, pOpus->pcm, (int)chunk, 0);
            if (result <= 0) {
                /* PLC itself failed (should not happen). Emit silence to keep the timeline. */
                memset(pOpus->pcm, 0, chunk * pOpus->channels * sizeof(float));
                result = (int)chunk;
            }

            pOpus->pendingConceal -= (dropus_uint32)result;
            pOpus->totalPCMFramesConcealed += (dropus_uint32)result;
            frames = (dropus_uint32)result;
            isConceal = DROPUS_TRUE;
        } else {
            dropus_uint32 packetSamples;
            int result;

            if (!dropus__read_next_packet(pOpus)) {
                pOpus->atEnd = DROPUS_TRUE;
                return 0;
            }

            packetSamples = dropus__packet_samples(pOpus->packet, pOpus->packetSize);
            if (packetSamples == 0) {
                /* Invalid packet header: conceal one 20ms frame and move on. */
                pOpus->totalPacketsLost += 1;
                pOpus->pendingConceal += 960;
                continue;
            }

            result = g_dropusOpus.decode_float(pOpus->pOpusDecoder, pOpus->packet, (dropus_int32)pOpus->packetSize, pOpus->pcm, (int)packetSamples, 0);
            if (result <= 0) {
                /* libopus rejected the packet: conceal and continue with the next one. */
                pOpus->totalPacketsLost += 1;
                pOpus->pendingConceal += packetSamples;
                continue;
            }

            frames = (dropus_uint32)result;
        }

        /* Trim to the granule position of the page the packet completed on. This handles the
        end-of-stream trimming encoded in the final page's granule position. Concealed frames
        are mid-stream by construction and are not trimmed. */
        if (!isConceal && pOpus->packetGranuleValid) {
            dropus_int64 remaining = pOpus->packetGranule - (dropus_int64)pOpus->decodedTotal;
            if (remaining < 0) {
                remaining = 0;
            }
            if (frames > (dropus_uint32)remaining) {
                frames = (dropus_uint32)remaining;
            }
        }

        pOpus->decodedTotal += frames;

        /* Discard the encoder pre-skip. */
        if (pOpus->preSkipRemaining > 0) {
            dropus_uint32 toSkip = (pOpus->preSkipRemaining < frames) ? (dropus_uint32)pOpus->preSkipRemaining : frames;
            pOpus->preSkipRemaining -= toSkip;
            frames -= toSkip;
            if (frames == 0) {
                continue;
            }
            memmove(pOpus->pcm, pOpus->pcm + (size_t)toSkip * pOpus->channels, frames * pOpus->channels * sizeof(float));
        }

        if (frames == 0) {
            continue;
        }

        dropus__apply_gain(pOpus, pOpus->pcm, frames);

        pOpus->pcmFramesRemaining = frames;
        pOpus->pcmFramesConsumed = 0;
        framesAvailable = frames;
        break;
    }

    return framesAvailable;
}


static dropus_bool32 dropus__init_decoder(dropus* pOpus)
{
    /* Read header and comment packets. */
    while (pOpus->headerStage < 2) {
        if (!dropus__read_next_packet(pOpus)) {
            return DROPUS_FALSE;
        }

        if (pOpus->headerStage == 0) {
            const dropus_uint8* pHead = pOpus->packet;
            dropus_uint32 inputSampleRate;
            dropus_uint32 mappingFamily;

            if (pOpus->packetSize < 19 || memcmp(pHead, "OpusHead", 8) != 0) {
                return DROPUS_FALSE;    /* Not an Ogg Opus stream. */
            }
            if (pHead[8] != 1) {
                return DROPUS_FALSE;    /* Unsupported OpusHead version. */
            }

            pOpus->channels = pHead[9];
            pOpus->preSkipInPCMFrames = dropus__read_u16le(pHead + 10);
            inputSampleRate = dropus__read_u32le(pHead + 12);   /* Informational only. Output is always 48kHz. */
            pOpus->outputGain = (dropus_int16)dropus__read_u16le(pHead + 16);
            mappingFamily = pHead[18];

            (void)inputSampleRate;

            if (mappingFamily != 0 || (pOpus->channels != 1 && pOpus->channels != 2)) {
                return DROPUS_FALSE;    /* Only channel mapping family 0 (mono/stereo) is supported. */
            }

            pOpus->sampleRate = DROPUS_SAMPLE_RATE;
            pOpus->preSkipRemaining = pOpus->preSkipInPCMFrames;
            pOpus->outputGainLinear = (float)pow(10.0, (double)pOpus->outputGain / 5120.0);

            {
                int error = 0;
                pOpus->pOpusDecoder = g_dropusOpus.decoder_create(DROPUS_SAMPLE_RATE, (int)pOpus->channels, &error);
                if (pOpus->pOpusDecoder == NULL || error != 0) {
                    return DROPUS_FALSE;
                }
            }

            pOpus->headerStage = 1;
        } else {
            /* OpusTags. Validated loosely and skipped. */
            if (pOpus->packetSize < 8 || memcmp(pOpus->packet, "OpusTags", 8) != 0) {
                return DROPUS_FALSE;
            }
            pOpus->headerStage = 2;
        }
    }

    return DROPUS_TRUE;
}


DROPUS_API void dropus_version(dropus_uint32* pMajor, dropus_uint32* pMinor, dropus_uint32* pRevision)
{
    if (pMajor != NULL) {
        *pMajor = DROPUS_VERSION_MAJOR;
    }
    if (pMinor != NULL) {
        *pMinor = DROPUS_VERSION_MINOR;
    }
    if (pRevision != NULL) {
        *pRevision = DROPUS_VERSION_REVISION;
    }
}

DROPUS_API const char* dropus_version_string(void)
{
    return DROPUS_VERSION_STRING;
}


/* Shared initialization. Assumes *pOpus has been zeroed and any state the read callback
depends on (e.g. the memory block for dropus_init_memory()) has already been set. */
static dropus_bool32 dropus__init(dropus* pOpus, dropus_read_proc onRead, void* pUserData, const dropus_allocation_callbacks* pAllocationCallbacks)
{
    pOpus->onRead = onRead;
    pOpus->pUserData = pUserData;
    if (pAllocationCallbacks != NULL) {
        pOpus->allocationCallbacks = *pAllocationCallbacks;
    }

    if (!dropus__load_opus()) {
        return DROPUS_FALSE;    /* libopus is not available on this system. */
    }

    if (!dropus__init_decoder(pOpus)) {
        dropus_uninit(pOpus);
        return DROPUS_FALSE;
    }

    return DROPUS_TRUE;
}

DROPUS_API dropus_bool32 dropus_init(dropus* pOpus, dropus_read_proc onRead, void* pUserData, const dropus_allocation_callbacks* pAllocationCallbacks)
{
    if (pOpus == NULL || onRead == NULL) {
        return DROPUS_FALSE;
    }

    memset(pOpus, 0, sizeof(*pOpus));

    return dropus__init(pOpus, onRead, pUserData, pAllocationCallbacks);
}


#ifndef DR_OPUS_NO_STDIO
static size_t dropus__on_read_file(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    return fread(pBufferOut, 1, bytesToRead, (FILE*)pUserData);
}

DROPUS_API dropus_bool32 dropus_init_file(dropus* pOpus, const char* pFilePath, const dropus_allocation_callbacks* pAllocationCallbacks)
{
    FILE* pFile;
    dropus_bool32 result;

    if (pFilePath == NULL) {
        return DROPUS_FALSE;
    }

    pFile = fopen(pFilePath, "rb");
    if (pFile == NULL) {
        return DROPUS_FALSE;
    }

    result = dropus_init(pOpus, dropus__on_read_file, pFile, pAllocationCallbacks);
    if (!result) {
        fclose(pFile);
        return DROPUS_FALSE;
    }

    pOpus->pFile = pFile;
    return DROPUS_TRUE;
}
#endif  /* DR_OPUS_NO_STDIO */


static size_t dropus__on_read_memory(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    dropus* pOpus = (dropus*)pUserData;
    size_t bytesRemaining = pOpus->memory.dataSize - pOpus->memory.currentReadPos;
    size_t bytesToCopy = (bytesToRead < bytesRemaining) ? bytesToRead : bytesRemaining;

    if (bytesToCopy > 0) {
        memcpy(pBufferOut, pOpus->memory.pData + pOpus->memory.currentReadPos, bytesToCopy);
        pOpus->memory.currentReadPos += bytesToCopy;
    }

    return bytesToCopy;
}

DROPUS_API dropus_bool32 dropus_init_memory(dropus* pOpus, const void* pData, size_t dataSize, const dropus_allocation_callbacks* pAllocationCallbacks)
{
    if (pOpus == NULL || pData == NULL || dataSize == 0) {
        return DROPUS_FALSE;
    }

    memset(pOpus, 0, sizeof(*pOpus));

    /* The read callback depends on the memory block, so it must be set up before
    the decoder starts reading. */
    pOpus->memory.pData = (const dropus_uint8*)pData;
    pOpus->memory.dataSize = dataSize;
    pOpus->memory.currentReadPos = 0;

    return dropus__init(pOpus, dropus__on_read_memory, pOpus, pAllocationCallbacks);
}


DROPUS_API void dropus_uninit(dropus* pOpus)
{
    if (pOpus == NULL) {
        return;
    }

    if (pOpus->pOpusDecoder != NULL && g_dropusOpus.decoder_destroy != NULL) {
        g_dropusOpus.decoder_destroy(pOpus->pOpusDecoder);
        pOpus->pOpusDecoder = NULL;
    }

#ifndef DR_OPUS_NO_STDIO
    if (pOpus->pFile != NULL) {
        fclose((FILE*)pOpus->pFile);
        pOpus->pFile = NULL;
    }
#endif
}


DROPUS_API dropus_uint64 dropus_read_pcm_frames_f32(dropus* pOpus, dropus_uint64 framesToRead, float* pBufferOut)
{
    dropus_uint64 framesRead = 0;

    if (pOpus == NULL || pBufferOut == NULL) {
        return 0;
    }

    while (framesRead < framesToRead) {
        if (pOpus->pcmFramesRemaining == 0) {
            if (dropus__fill_pcm(pOpus) == 0) {
                break;  /* End of stream. */
            }
        }

        {
            dropus_uint64 framesToCopy64 = framesToRead - framesRead;
            dropus_uint32 framesToCopy = (framesToCopy64 < pOpus->pcmFramesRemaining) ? (dropus_uint32)framesToCopy64 : pOpus->pcmFramesRemaining;
            dropus_uint32 channels = pOpus->channels;

            memcpy(pBufferOut + (size_t)framesRead * channels,
                   pOpus->pcm + (size_t)pOpus->pcmFramesConsumed * channels,
                   (size_t)framesToCopy * channels * sizeof(float));

            pOpus->pcmFramesConsumed += framesToCopy;
            pOpus->pcmFramesRemaining -= framesToCopy;
            framesRead += framesToCopy;
        }
    }

    return framesRead;
}


static dropus_int16 dropus__f32_to_s16(float x)
{
    int s;
    x = x * 32768.0f;
    x = (x >= 0.0f) ? x + 0.5f : x - 0.5f;
    s = (int)x;
    if (s > 32767) {
        s = 32767;
    }
    if (s < -32768) {
        s = -32768;
    }
    return (dropus_int16)s;
}

DROPUS_API dropus_uint64 dropus_read_pcm_frames_s16(dropus* pOpus, dropus_uint64 framesToRead, dropus_int16* pBufferOut)
{
    dropus_uint64 framesRead = 0;

    if (pOpus == NULL || pBufferOut == NULL) {
        return 0;
    }

    while (framesRead < framesToRead) {
        if (pOpus->pcmFramesRemaining == 0) {
            if (dropus__fill_pcm(pOpus) == 0) {
                break;  /* End of stream. */
            }
        }

        {
            dropus_uint64 framesToCopy64 = framesToRead - framesRead;
            dropus_uint32 framesToCopy = (framesToCopy64 < pOpus->pcmFramesRemaining) ? (dropus_uint32)framesToCopy64 : pOpus->pcmFramesRemaining;
            dropus_uint32 channels = pOpus->channels;
            const float* pSrc = pOpus->pcm + (size_t)pOpus->pcmFramesConsumed * channels;
            dropus_int16* pDst = pBufferOut + (size_t)framesRead * channels;
            dropus_uint32 sampleCount = framesToCopy * channels;
            dropus_uint32 i;

            for (i = 0; i < sampleCount; ++i) {
                pDst[i] = dropus__f32_to_s16(pSrc[i]);
            }

            pOpus->pcmFramesConsumed += framesToCopy;
            pOpus->pcmFramesRemaining -= framesToCopy;
            framesRead += framesToCopy;
        }
    }

    return framesRead;
}

#endif  /* dr_opus_c */
#endif  /* DR_OPUS_IMPLEMENTATION */
