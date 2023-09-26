#pragma once

#include "Macros.hpp"

#include "miniaudio/miniaudio.h"

// ================================================================================
#pragma region Defines and Macros
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
#pragma endregion
// ================================================================================

// ================================================================================
#pragma region Types and structures definitions
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
	AUDIO_BUFFER_USAGE_STATIC = 0,
	AUDIO_BUFFER_USAGE_STREAM
} AudioBufferUsage;

// Structs ------------------------------------------------------------------------

typedef struct riqAudioProcessor
{
	AudioCallback process;          // Processor callback function
	riqAudioProcessor* next;        // Next audio processor on the list
	riqAudioProcessor* prev;        // Previous audio processor on the list
} riqAudioProcessor;

struct riqAudioBuffer
{
	ma_data_converter converter;    // Audio data converter

	AudioCallback callback;         // Audio buffer callback for buffer filling on audio threads
	riqAudioProcessor* processor;   // Audio processor

	float volume;                   // Audio buffer volume
	float pitch;                    // Audio buffer pitch
	float pan;                      // Audio buffer pan (0.0f to 1.0f)

	bool playing;                   // Audio buffer state: AUDIO_PLAYING
	bool paused;                    // Audio buffer state: AUDIO_PAUSED
	bool looping;                   // Audio buffer looping, default to true for AudioStreams
	int usage;                      // Audio buffer usage mode: STATIC or STREAM

	bool isSubBufferProcessed[2];   // SubBuffer processed (virtual double buffer)
	unsigned int sizeInFrames;      // Total buffer size in frames
	unsigned int frameCursorPos;    // Frame cursor position
	unsigned int framesProcessed;   // Total frames processed in this buffer (required for play timing)

	unsigned char* data;            // Data buffer, on music stream keeps filling

	riqAudioBuffer* next;           // Next audio buffer on the list
	riqAudioBuffer* prev;           // Previous audio buffer on the list
};

typedef struct AudioStream
{
	riqAudioBuffer* buffer;         // Pointer to internal data used by the audio system
	riqAudioProcessor* processor;   // Pointer to internal data processor, useful for audio effects

	unsigned int sampleRate;        // Frequency (samples per second)
	unsigned int sampleSize;        // Bit depth (bits per sample): 8, 16, 32 (24 not supported)
	unsigned int channels;          // Number of channels (1-mono, 2-stereo, ...)
} AudioStream;

typedef riqAudioBuffer AudioBuffer;

typedef struct AudioData
{
	struct 
	{
		ma_context context;         // miniaudio context data
		ma_device device;           // miniaudio device
		ma_mutex lock;              // miniaudio mutex lock
		bool isReady;               // Check if audio device is ready
		size_t pcmBufferSize;       // Preallocated buffer size
		void* pcmBuffer;            // Preallocated buffer to read audio data from file/memory
	} System;
	struct
	{
		AudioBuffer* first;         // Pointer to first AudioBuffer in the list
		AudioBuffer* last;          // Pointer to last AudioBuffer in the list
		int defaultSize = 0;        // Default audio buffer size for audio streams
	} Buffer;
	riqAudioProcessor* mixedProcessor = NULL;

} AudioData;

// Audio wave data
typedef struct Wave
{
	unsigned int frameCount;
	unsigned int sampleRate;
	unsigned int sampleSize;
	unsigned int channels;
	void* data;
} Wave;

// Sound effects
typedef struct Sound
{
	AudioStream stream;
	unsigned int frameCount;
} Sound;

// Music, audio stream, anything longer than ~10 seconds should be streamed
typedef struct Music
{
	AudioStream stream;
	unsigned int frameCount;
	bool looping;

	int ctxType;                // Type of music context (audio filetype)
	void* ctxData;              // Audio context data, depends on type
} Music;

// ================================================================================
#pragma endregion
// ================================================================================

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

extern "C"
{
DllExport void RiqInitAudioDevice(void);
DllExport void RiqCloseAudioDevice(void);
DllExport bool IsRiqReady();

DllExport Sound RiqLoadSound(const char* filePath);
DllExport Sound RiqLoadSoundFromWave(Wave wave);
DllExport void RiqUnloadSound(Sound sound);
DllExport void RiqPlaySound(Sound sound);


DllExport Wave RiqLoadWave(const char* filePath);
DllExport Wave RiqLoadWaveFromMemory(const char* fileType, const unsigned char* fileData, int dataSize);
DllExport void RiqUnloadWave(Wave wave);
}

static const char* GetFileExtension(const char* fileName);
unsigned char* LoadFileData(const char* fileName, unsigned int* bytesRead);