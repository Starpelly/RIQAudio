#pragma once

#include "Macros.hpp"

#include "miniaudio/miniaudio.h"

// ================================================================================
// Defines and Macros
// ================================================================================

#ifndef AUDIO_DEVICE_FORMAT
#define AUDIO_DEVICE_FORMAT    ma_format_f32    // Device output format (float-32bit)
#endif
#ifndef AUDIO_DEVICE_CHANNELS
#define AUDIO_DEVICE_CHANNELS              2    // Device output channels: stereo
#endif
#ifndef AUDIO_DEVICE_SAMPLE_RATE
#define AUDIO_DEVICE_SAMPLE_RATE           0    // Device output sample rate
#endif

#ifndef MAX_AUDIO_BUFFER_POOL_CHANNELS
#define MAX_AUDIO_BUFFER_POOL_CHANNELS    16    // Audio pool channels
#endif


// ================================================================================
// Types and structures definitions
// ================================================================================


typedef void (*AudioCallback)(void* bufferdata, unsigned int frames);

// Enums --------------------------------------------------------------------------

typedef enum
{
    MUSIC_AUDIO_NONE = 0,
    MUSIC_AUDIO_WAV,
    MUSIC_AUDIO_OGG,
    MUSIC_AUDIO_FLAC,
    MUSIC_AUDIO_MP3
} MusicContextType;

typedef enum
{
    LOG_ALL = 0,
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL,
    LOG_NONE
} TraceLogLevel;

typedef enum {
    AUDIO_BUFFER_USAGE_STATIC = 0,
    AUDIO_BUFFER_USAGE_STREAM
} AudioBufferUsage;

// Structs ------------------------------------------------------------------------

typedef struct riqAudioProcessor
{
    AudioCallback process;
    riqAudioProcessor* next;
    riqAudioProcessor* prev;
} riqAudioProcessor;

typedef struct riqAudioBuffer
{
    ma_data_converter converter;

    AudioCallback callback;
    riqAudioProcessor* processor;

    float volume;
    float pitch;
    float pan;

    bool playing;
    bool paused;
    bool looping;
    int usage;

    bool isSubBufferProcessed[2];
    unsigned int sizeInFrames;
    unsigned int frameCursorPos;
    unsigned int framesProcessed;

    unsigned char* data;

    riqAudioBuffer* next;
    riqAudioBuffer* prev;
} riqAudioBuffer;


typedef struct AudioStream
{
    riqAudioBuffer* buffer;
    riqAudioProcessor* processor;

    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channels;
} AudioStream;

typedef riqAudioBuffer AudioBuffer;

typedef struct AudioData
{
    struct {
        ma_context context;         // miniaudio context data
        ma_device device;           // miniaudio device
        ma_mutex lock;              // miniaudio mutex lock
        bool isReady;               // Check if audio device is ready
        size_t pcmBufferSize;       // Pre-allocated buffer size
        void* pcmBuffer;            // Pre-allocated buffer to read audio data from file/memory
    } System;
    struct {
        AudioBuffer* first;         // Pointer to first AudioBuffer in the list
        AudioBuffer* last;          // Pointer to last AudioBuffer in the list
        int defaultSize;            // Default audio buffer size for audio streams
    } Buffer;
    riqAudioProcessor* mixedProcessor;

} AudioData;


typedef struct Wave {
    unsigned int frameCount;
    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channels;
    void* data;
} Wave;

typedef struct Sound {
    AudioStream stream;
    unsigned int frameCount;
} Sound;

// ================================================================================
//
// ================================================================================


#ifdef __cplusplus
extern "C" {
#endif

AudioBuffer* LoadAudioBuffer(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 sizeInFrames, int usage);
void UnloadAudioBuffer(AudioBuffer* buffer);

bool IsAudioBufferPlaying(AudioBuffer* buffer);
void PlayAudioBuffer(AudioBuffer* buffer);
void StopAudioBuffer(AudioBuffer* buffer);
void PauseAudioBuffer(AudioBuffer* buffer);
void ResumeAudioBuffer(AudioBuffer* buffer);
// void SetAudioBufferVolume(AudioBuffer* buffer, float volume);
// void SetAudioBufferPitch(AudioBuffer* buffer, float pitch);
// void SetAudioBufferPan(AudioBuffer* buffer, float pan);
void TrackAudioBuffer(AudioBuffer* buffer);
void UntrackAudioBuffer(AudioBuffer* buffer);

DllExport void RiqInitAudioDevice(void);
DllExport void RiqCloseAudioDevice(void);
DllExport bool IsRiqReady();

DllExport Sound RiqLoadSound(const char* filePath);
DllExport Sound RiqLoadSoundFromWave(Wave wave);
DllExport void RiqUnloadSound(Sound sound);
DllExport void RiqPlaySound(Sound sound);


Wave LoadWave(const char* filePath);
Wave LoadWaveFromMemory(const unsigned char* fileData, int dataSize);
void UnloadWave(Wave wave);

#ifdef __cplusplus
}
#endif

unsigned char* LoadFileData(const char* fileName, unsigned int* bytesRead);