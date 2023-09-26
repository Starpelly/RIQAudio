#include "RIQAudio.hpp"
#include "UnityHelpers.hpp"

// Rust users cry
#ifndef RIQ_MALLOC
    #define RIQ_MALLOC(sz)          malloc(sz);
#endif
#ifndef RIQ_CALLOC
    #define RIQ_CALLOC(n,sz)        calloc(n,sz)
#endif
#ifndef RIQ_REALLOC
    #define RIQ_REALLOC(ptr,sz)     realloc(ptr,sz)
#endif
#ifndef RIQ_FREE
    #define RIQ_FREE(ptr)           free(ptr)
#endif

#pragma region File Formats

#define SUPPORT_FILEFORMAT_OGG
#define SUPPORT_FILEFORMAT_WAV

#if defined(SUPPORT_FILEFORMAT_OGG)
#define STB_VORBIS_IMPLEMENTATION
#include "miniaudio/extras/stb_vorbis.h" // Vorbis decoding
#endif

#if defined(SUPPORT_FILEFORMAT_WAV)
    #define DRWAV_MALLOC RIQ_MALLOC
    #define DRWAV_REALLOC RIQ_REALLOC
    #define DRWAV_FREE RIQ_FREE

    #define DR_WAV_IMPLEMENTATION
    #include "miniaudio/extras/dr_wav.h" // WAV decoding
#endif

#pragma endregion

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

ma_result result;
ma_decoder decoder;
ma_device_config deviceConfig;
ma_device device;

// Global audio context
static AudioData AUDIO =
{
};

static void OnLog(void* pUserData, ma_uint32 level, const char* pMessage);
static void OnSendAudioDataToDevice(ma_device* pDevice, void* pFramesOut, const void* pFramesInput, ma_uint32 frameCount);

void RiqInitAudioDevice(void)
{

    ma_context_config ctxConfig = ma_context_config_init();
    ma_log_callback_init(OnLog, NULL);

    ma_result result = ma_context_init(NULL, 0, &ctxConfig, &AUDIO.System.context);
    if (result != MA_SUCCESS)
    {
        DEBUG_LOG(unityLogPtr, "RIQAudio: Failed to initialize context!");
        return;
    }

    // Initialize audio device
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = NULL;
    config.playback.format = AUDIO_DEVICE_FORMAT;
    config.playback.channels = AUDIO_DEVICE_CHANNELS;
    
    config.capture.pDeviceID = NULL;
    config.capture.format = ma_format_s16;
    config.capture.channels = 1;
    config.sampleRate = AUDIO_DEVICE_SAMPLE_RATE;

    config.dataCallback = OnSendAudioDataToDevice;
    config.pUserData = NULL;

    result = ma_device_init(&AUDIO.System.context, &config, &AUDIO.System.device);
    if (result != MA_SUCCESS)
    {
        DEBUG_ERROR(unityLogPtr, "RIQAudio: Failed to initialize playback device!");
        return;
    }

    result = ma_device_start(&AUDIO.System.device);
    if (result != MA_SUCCESS)
    {
        DEBUG_ERROR(unityLogPtr, "RIQAudio: Failed to start playback device!");
        ma_device_uninit(&AUDIO.System.device);
        ma_context_uninit(&AUDIO.System.context);
        return;
    }

    if (ma_mutex_init(&AUDIO.System.lock) != MA_SUCCESS)
    {
        DEBUG_ERROR(unityLogPtr, "RIQAudio: Failed to create mutex for mixing!");

        ma_device_uninit(&AUDIO.System.device);
        ma_context_uninit(&AUDIO.System.context);
        return;
    }

    for (int i = 0; i < 16; i++)
    {
        // Get back to this
        // AUDIO.MultiChannel.pool[i] = LoadAudioBuffer(AUDIO_DEVICE_FORMAT, AUDIO_DEVICE_CHANNELS, AUDIO.System.device.sampleRate, 0, 0);
    }

    AUDIO.System.isReady = true;

    DEBUG_LOG(unityLogPtr, "RIQAudio: Device initialized successfully!");
}

bool IsRiqReady()
{
    return AUDIO.System.isReady;
}

void RiqCloseAudioDevice(void)
{
    if (AUDIO.System.isReady)
    {
        ma_mutex_uninit(&AUDIO.System.lock);
        ma_device_uninit(&AUDIO.System.device);
        ma_context_uninit(&AUDIO.System.context);

        AUDIO.System.isReady = false;

        RIQ_FREE(AUDIO.System.pcmBuffer);

        DEBUG_LOG(unityLogPtr, "RIQAudio: Device closed successfully!");
    }
    else DEBUG_ERROR(unityLogPtr, "RIQAudio: Device could not be closed, not currently initialized!");
}



#pragma region AudioBuffer

AudioBuffer* LoadAudioBuffer(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 sizeInFrames, int usage)
{
    AudioBuffer* audioBuffer = (AudioBuffer*)RIQ_CALLOC(1, sizeof(AudioBuffer));

    if (audioBuffer == NULL)
    {
        DEBUG_WARNING(unityLogPtr, "AUDIO: Failed to allocate memory for buffer");
        return NULL;
    }

    if (sizeInFrames > 0) audioBuffer->data = (unsigned char*)RIQ_CALLOC(sizeInFrames * channels * ma_get_bytes_per_sample(format), 1);

    // Audio data runs through a format converter
    ma_data_converter_config converterConfig = ma_data_converter_config_init(format, AUDIO_DEVICE_FORMAT, channels, AUDIO_DEVICE_CHANNELS, sampleRate, AUDIO.System.device.sampleRate);
    converterConfig.allowDynamicSampleRate = true;

    ma_result result = ma_data_converter_init(&converterConfig, NULL, &audioBuffer->converter);

    if (result != MA_SUCCESS)
    {
        DEBUG_WARNING(unityLogPtr, "AUDIO: Failed to create data conversion pipeline");
        RIQ_FREE(audioBuffer);
        return NULL;
    }

    // Init audio buffer values
    audioBuffer->volume = 1.0f;
    audioBuffer->pitch = 1.0f;
    audioBuffer->pan = 0.5f;

    audioBuffer->callback = NULL;
    audioBuffer->processor = NULL;

    audioBuffer->playing = false;
    audioBuffer->paused = false;
    audioBuffer->looping = false;

    audioBuffer->usage = usage;
    audioBuffer->frameCursorPos = 0;
    audioBuffer->sizeInFrames = sizeInFrames;

    // Buffers should be marked as processed by default so that a call to
    // UpdateAudioStream() immediately after initialization works correctly
    audioBuffer->isSubBufferProcessed[0] = true;
    audioBuffer->isSubBufferProcessed[1] = true;

    TrackAudioBuffer(audioBuffer);

    return audioBuffer;
}

void UnloadAudioBuffer(AudioBuffer* buffer)
{
    if (buffer != NULL)
    {
        ma_data_converter_uninit(&buffer->converter, NULL);
        UntrackAudioBuffer(buffer);
        RIQ_FREE(buffer->data);
        RIQ_FREE(buffer);
    }
}

bool IsAudioBufferPlaying(AudioBuffer* buffer)
{
    bool result = false;

    if (buffer != NULL) result = (buffer->playing && !buffer->paused);

    return result;
}

void PlayAudioBuffer(AudioBuffer* buffer)
{
    if (buffer != NULL)
    {
        buffer->playing = true;
        buffer->paused = false;
        buffer->frameCursorPos = 0;
    }
}

void StopAudioBuffer(AudioBuffer* buffer)
{
    if (buffer != NULL)
    {
        if (IsAudioBufferPlaying(buffer))
        {
            buffer->playing = false;
            buffer->paused = false;
            buffer->frameCursorPos = 0;
            buffer->framesProcessed = 0;
            buffer->isSubBufferProcessed[0] = true;
            buffer->isSubBufferProcessed[1] = true;
        }
    }
}

void PauseAudioBuffer(AudioBuffer* buffer)
{
    if (buffer != NULL) buffer->paused = true;
}

void ResumeAudioBuffer(AudioBuffer* buffer)
{
    if (buffer != NULL) buffer->paused = false;
}

void TrackAudioBuffer(AudioBuffer* buffer)
{
    ma_mutex_lock(&AUDIO.System.lock);
    {
        if (AUDIO.Buffer.first == NULL) AUDIO.Buffer.first = buffer;
        else
        {
            AUDIO.Buffer.last->next = buffer;
            buffer->prev = AUDIO.Buffer.last;
        }

        AUDIO.Buffer.last = buffer;
    }
    ma_mutex_unlock(&AUDIO.System.lock);
}

void UntrackAudioBuffer(AudioBuffer* buffer)
{
    ma_mutex_lock(&AUDIO.System.lock);
    {
        if (buffer->prev == NULL) AUDIO.Buffer.first = buffer->next;
        else buffer->prev->next = buffer->next;

        if (buffer->next == NULL) AUDIO.Buffer.last = buffer->prev;
        else buffer->next->prev = buffer->prev;

        buffer->prev = NULL;
        buffer->next = NULL;
    }
    ma_mutex_unlock(&AUDIO.System.lock);
}

#pragma endregion

#pragma region Sound

Sound RiqLoadSound(const char* filePath)
{
    Wave wave = RiqLoadWave(filePath);

    Sound sound = RiqLoadSoundFromWave(wave);

    RiqUnloadWave(wave);

    return sound;
}


Sound RiqLoadSoundFromWave(Wave wave)
{
    Sound sound = { 0 };

    if (wave.data != NULL)
    {
		// When using miniaudio we need to do our own mixing.
		// To simplify this we need convert the format of each sound to be consistent with
		// the format used to open the playback AUDIO.System.device. We can do this two ways:
		//
		//   1) Convert the whole sound in one go at load time (here).
		//   2) Convert the audio data in chunks at mixing time.
		//
		// First option has been selected, format conversion is done on the loading stage.
		// The downside is that it uses more memory if the original sound is u8 or s16.
        ma_format formatIn = ((wave.sampleSize == 8) ? ma_format_u8 : ((wave.sampleSize == 16) ? ma_format_s16 : ma_format_f32));
        ma_uint32 frameCountIn = wave.frameCount;

        ma_uint32 frameCount = (ma_uint32)ma_convert_frames(NULL, 0, AUDIO_DEVICE_FORMAT, AUDIO_DEVICE_CHANNELS, AUDIO.System.device.sampleRate, NULL, frameCountIn, formatIn, wave.channels, wave.sampleRate);
        if (frameCount == 0) DEBUG_WARNING(unityLogPtr, "SOUND: Failed to get frame count for format conversion");

        AudioBuffer* audioBuffer = LoadAudioBuffer(AUDIO_DEVICE_FORMAT, AUDIO_DEVICE_CHANNELS, AUDIO.System.device.sampleRate, frameCount, AUDIO_BUFFER_USAGE_STATIC);
        if (audioBuffer == NULL)
        {
            DEBUG_WARNING(unityLogPtr, "SOUND: Failed to create buffer");
            return sound; // Early return to avoid dereferencing the audioBuffer null pointer.
        }

        frameCount = (ma_uint32)ma_convert_frames(audioBuffer->data, frameCount, AUDIO_DEVICE_FORMAT, AUDIO_DEVICE_CHANNELS, AUDIO.System.device.sampleRate, wave.data, frameCountIn, formatIn, wave.channels, wave.sampleRate);
        if (frameCount == 0) DEBUG_WARNING(unityLogPtr, "SOUND: Failed format conversion");

        sound.frameCount = frameCount;
        sound.stream.sampleRate = AUDIO.System.device.sampleRate;
        sound.stream.sampleSize = 32;
        sound.stream.channels = AUDIO_DEVICE_CHANNELS;
        sound.stream.buffer = audioBuffer;
    }

    return sound;
}

void RiqUnloadSound(Sound sound)
{
    UnloadAudioBuffer(sound.stream.buffer);
}


void RiqPlaySound(Sound sound)
{
    PlayAudioBuffer(sound.stream.buffer);
}

#pragma endregion

#pragma region Wave

Wave RiqLoadWave(const char* filePath)
{
    Wave wave = { 0 };

    unsigned int fileSize = 0;
    unsigned char* fileData = LoadFileData(filePath, &fileSize);

    if (fileData != NULL) wave = RiqLoadWaveFromMemory(GetFileExtension(filePath), fileData, fileSize);

    RIQ_FREE(fileData);

    return wave;
}

Wave RiqLoadWaveFromMemory(const char* fileType, const unsigned char* fileData, int dataSize)
{
    Wave wave = { 0 };

    if (false) {}
#if defined(SUPPORT_FILEFORMAT_WAV)
    else if (strcmp(fileType, ".wav") == 0)
    {
		drwav wav = { 0 };
		bool success = drwav_init_memory(&wav, fileData, dataSize, NULL);

		if (success)
		{
			wave.frameCount = (unsigned int)wav.totalPCMFrameCount;
			wave.sampleRate = wav.sampleRate;
			wave.sampleSize = 16;
			wave.channels = wav.channels;
			wave.data = (short*)RIQ_MALLOC(wave.frameCount * wave.channels * sizeof(short));

			// NOTE: We are forcing conversion to 16bit sample size on reading
			drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, (drwav_int16*)wave.data);
		}
		else DEBUG_WARNING(unityLogPtr, "WAVE: Failed to load WAV data!");

		drwav_uninit(&wav);
    }
#endif
#if defined(SUPPORT_FILEFORMAT_OGG)
    else if (strcmp(fileType, ".ogg") == 0)
    {
        stb_vorbis* oggData = stb_vorbis_open_memory((unsigned char*)fileData, dataSize, NULL, NULL);

        if (oggData != NULL)
        {
            stb_vorbis_info info = stb_vorbis_get_info(oggData);

            wave.sampleRate = info.sample_rate;
            wave.sampleSize = 16;
            wave.channels = info.channels;
            wave.frameCount = (unsigned int)stb_vorbis_stream_length_in_samples(oggData);
            wave.data = (short*)RIQ_MALLOC(wave.frameCount * wave.channels * sizeof(short));

            stb_vorbis_get_samples_short_interleaved(oggData, info.channels, (short*)wave.data, wave.frameCount * wave.channels);
            stb_vorbis_close(oggData);
        }
        else DEBUG_WARNING(unityLogPtr, "WAVE: Failed to load OGG data!");
    }
#endif
    else DEBUG_WARNING(unityLogPtr, "WAVE: Data format not supported!");

    DEBUG_LOG_FMT(unityLogPtr, "WAVE: Data loaded successfully (%i Hz, %i bit, %i channels)", wave.sampleRate, wave.sampleSize, wave.channels);

    return wave;
}

void RiqUnloadWave(Wave wave)
{
    RIQ_FREE(wave.data);
}

#pragma endregion

#pragma region rAudioFunctions

// Main mixing function, pretty simple in this project, just an accumulation
// NOTE: framesOut is both an input and an output, it is initially filled with zeros outside of this function
static void MixAudioFrames(float* framesOut, const float* framesIn, ma_uint32 frameCount, AudioBuffer* buffer)
{
    const float localVolume = buffer->volume;
    const ma_uint32 channels = AUDIO.System.device.playback.channels;

    if (channels == 2)  // We consider panning
    {
        const float left = buffer->pan;
        const float right = 1.0f - left;

        // Fast sine approximation in [0..1] for pan law: y = 0.5f*x*(3 - x*x);
        const float levels[2] = { localVolume * 0.5f * left * (3.0f - left * left), localVolume * 0.5f * right * (3.0f - right * right) };

        float* frameOut = framesOut;
        const float* frameIn = framesIn;

        for (ma_uint32 frame = 0; frame < frameCount; frame++)
        {
            frameOut[0] += (frameIn[0] * levels[0]);
            frameOut[1] += (frameIn[1] * levels[1]);

            frameOut += 2;
            frameIn += 2;
        }
    }
    else  // We do not consider panning
    {
        for (ma_uint32 frame = 0; frame < frameCount; frame++)
        {
            for (ma_uint32 c = 0; c < channels; c++)
            {
                float* frameOut = framesOut + (frame * channels);
                const float* frameIn = framesIn + (frame * channels);

                // Output accumulates input multiplied by volume to provided output (usually 0)
                frameOut[c] += (frameIn[c] * localVolume);
            }
        }
    }
}

// Reads audio data from an AudioBuffer object in internal format.
static ma_uint32 ReadAudioBufferFramesInInternalFormat(AudioBuffer* audioBuffer, void* framesOut, ma_uint32 frameCount)
{
    // Using audio buffer callback
    if (audioBuffer->callback)
    {
        audioBuffer->callback(framesOut, frameCount);
        audioBuffer->framesProcessed += frameCount;

        return frameCount;
    }

    ma_uint32 subBufferSizeInFrames = (audioBuffer->sizeInFrames > 1) ? audioBuffer->sizeInFrames / 2 : audioBuffer->sizeInFrames;
    ma_uint32 currentSubBufferIndex = audioBuffer->frameCursorPos / subBufferSizeInFrames;

    if (currentSubBufferIndex > 1) return 0;

    // Another thread can update the processed state of buffers, so
    // we just take a copy here to try and avoid potential synchronization problems
    bool isSubBufferProcessed[2] = { 0 };
    isSubBufferProcessed[0] = audioBuffer->isSubBufferProcessed[0];
    isSubBufferProcessed[1] = audioBuffer->isSubBufferProcessed[1];

    ma_uint32 frameSizeInBytes = ma_get_bytes_per_frame(audioBuffer->converter.formatIn, audioBuffer->converter.channelsIn);

    // Fill out every frame until we find a buffer that's marked as processed. Then fill the remainder with 0
    ma_uint32 framesRead = 0;
    while (1)
    {
        // We break from this loop differently depending on the buffer's usage
        //  - For static buffers, we simply fill as much data as we can
        //  - For streaming buffers we only fill half of the buffer that are processed
        //    Unprocessed halves must keep their audio data in-tact
        if (audioBuffer->usage == AUDIO_BUFFER_USAGE_STATIC)
        {
            if (framesRead >= frameCount) break;
        }
        else
        {
            if (isSubBufferProcessed[currentSubBufferIndex]) break;
        }

        ma_uint32 totalFramesRemaining = (frameCount - framesRead);
        if (totalFramesRemaining == 0) break;

        ma_uint32 framesRemainingInOutputBuffer;
        if (audioBuffer->usage == AUDIO_BUFFER_USAGE_STATIC)
        {
            framesRemainingInOutputBuffer = audioBuffer->sizeInFrames - audioBuffer->frameCursorPos;
        }
        else
        {
            ma_uint32 firstFrameIndexOfThisSubBuffer = subBufferSizeInFrames * currentSubBufferIndex;
            framesRemainingInOutputBuffer = subBufferSizeInFrames - (audioBuffer->frameCursorPos - firstFrameIndexOfThisSubBuffer);
        }

        ma_uint32 framesToRead = totalFramesRemaining;
        if (framesToRead > framesRemainingInOutputBuffer) framesToRead = framesRemainingInOutputBuffer;

        memcpy((unsigned char*)framesOut + (framesRead * frameSizeInBytes), audioBuffer->data + (audioBuffer->frameCursorPos * frameSizeInBytes), framesToRead * frameSizeInBytes);
        audioBuffer->frameCursorPos = (audioBuffer->frameCursorPos + framesToRead) % audioBuffer->sizeInFrames;
        framesRead += framesToRead;

        // If we've read to the end of the buffer, mark it as processed
        if (framesToRead == framesRemainingInOutputBuffer)
        {
            audioBuffer->isSubBufferProcessed[currentSubBufferIndex] = true;
            isSubBufferProcessed[currentSubBufferIndex] = true;

            currentSubBufferIndex = (currentSubBufferIndex + 1) % 2;

            // We need to break from this loop if we're not looping
            if (!audioBuffer->looping)
            {
                StopAudioBuffer(audioBuffer);
                break;
            }
        }
    }

    // Zero-fill excess
    ma_uint32 totalFramesRemaining = (frameCount - framesRead);
    if (totalFramesRemaining > 0)
    {
        memset((unsigned char*)framesOut + (framesRead * frameSizeInBytes), 0, totalFramesRemaining * frameSizeInBytes);

        // For static buffers we can fill the remaining frames with silence for safety, but we don't want
        // to report those frames as "read". The reason for this is that the caller uses the return value
        // to know whether a non-looping sound has finished playback.
        if (audioBuffer->usage != AUDIO_BUFFER_USAGE_STATIC) framesRead += totalFramesRemaining;
    }

    return framesRead;
}

// Reads audio data from an AudioBuffer object in device format. Returned data will be in a format appropriate for mixing.
static ma_uint32 ReadAudioBufferFramesInMixingFormat(AudioBuffer* audioBuffer, float* framesOut, ma_uint32 frameCount)
{
    // What's going on here is that we're continuously converting data from the AudioBuffer's internal format to the mixing format, which
    // should be defined by the output format of the data converter. We do this until frameCount frames have been output. The important
    // detail to remember here is that we never, ever attempt to read more input data than is required for the specified number of output
    // frames. This can be achieved with ma_data_converter_get_required_input_frame_count().
    ma_uint8 inputBuffer[4096] = { 0 };
    ma_uint32 inputBufferFrameCap = sizeof(inputBuffer) / ma_get_bytes_per_frame(audioBuffer->converter.formatIn, audioBuffer->converter.channelsIn);

    ma_uint32 totalOutputFramesProcessed = 0;
    while (totalOutputFramesProcessed < frameCount)
    {
        ma_uint64 outputFramesToProcessThisIteration = frameCount - totalOutputFramesProcessed;
        ma_uint64 inputFramesToProcessThisIteration = 0;

        (void)ma_data_converter_get_required_input_frame_count(&audioBuffer->converter, outputFramesToProcessThisIteration, &inputFramesToProcessThisIteration);
        if (inputFramesToProcessThisIteration > inputBufferFrameCap)
        {
            inputFramesToProcessThisIteration = inputBufferFrameCap;
        }

        float* runningFramesOut = framesOut + (totalOutputFramesProcessed * audioBuffer->converter.channelsOut);

        /* At this point we can convert the data to our mixing format. */
        ma_uint64 inputFramesProcessedThisIteration = ReadAudioBufferFramesInInternalFormat(audioBuffer, inputBuffer, (ma_uint32)inputFramesToProcessThisIteration);    /* Safe cast. */
        ma_uint64 outputFramesProcessedThisIteration = outputFramesToProcessThisIteration;
        ma_data_converter_process_pcm_frames(&audioBuffer->converter, inputBuffer, &inputFramesProcessedThisIteration, runningFramesOut, &outputFramesProcessedThisIteration);

        totalOutputFramesProcessed += (ma_uint32)outputFramesProcessedThisIteration; /* Safe cast. */

        if (inputFramesProcessedThisIteration < inputFramesToProcessThisIteration)
        {
            break;  /* Ran out of input data. */
        }

        /* This should never be hit, but will add it here for safety. Ensures we get out of the loop when no input nor output frames are processed. */
        if (inputFramesProcessedThisIteration == 0 && outputFramesProcessedThisIteration == 0)
        {
            break;
        }
    }

    return totalOutputFramesProcessed;
}


static void OnSendAudioDataToDevice(ma_device* pDevice, void* pFramesOut, const void* pFramesInput, ma_uint32 frameCount)
{
    (void)pDevice;

    // Mixing is basically just an accumulation, we need to initialize the output buffer to 0
    memset(pFramesOut, 0, frameCount * pDevice->playback.channels * ma_get_bytes_per_sample(pDevice->playback.format));

    // Using a mutex here for thread-safety which makes things not real-time
    // This is unlikely to be necessary for this project, but may want to consider how you might want to avoid this
    ma_mutex_lock(&AUDIO.System.lock);
    {
        for (AudioBuffer* audioBuffer = AUDIO.Buffer.first; audioBuffer != NULL; audioBuffer = audioBuffer->next)
        {
            // Ignore stopped or paused sounds
            if (!audioBuffer->playing || audioBuffer->paused) continue;

            ma_uint32 framesRead = 0;

            while (1)
            {
                if (framesRead >= frameCount) break;

                // Just read as much data as we can from the stream
                ma_uint32 framesToRead = (frameCount - framesRead);

                while (framesToRead > 0)
                {
                    float tempBuffer[1024] = { 0 }; // Frames for stereo

                    ma_uint32 framesToReadRightNow = framesToRead;
                    if (framesToReadRightNow > sizeof(tempBuffer) / sizeof(tempBuffer[0]) / AUDIO_DEVICE_CHANNELS)
                    {
                        framesToReadRightNow = sizeof(tempBuffer) / sizeof(tempBuffer[0]) / AUDIO_DEVICE_CHANNELS;
                    }

                    ma_uint32 framesJustRead = ReadAudioBufferFramesInMixingFormat(audioBuffer, tempBuffer, framesToReadRightNow);
                    if (framesJustRead > 0)
                    {
                        float* framesOut = (float*)pFramesOut + (framesRead * AUDIO.System.device.playback.channels);
                        float* framesIn = tempBuffer;

                        // Apply processors chain if defined
                        riqAudioProcessor* processor = audioBuffer->processor;
                        while (processor)
                        {
                            processor->process(framesIn, framesJustRead);
                            processor = processor->next;
                        }

                        MixAudioFrames(framesOut, framesIn, framesJustRead, audioBuffer);

                        framesToRead -= framesJustRead;
                        framesRead += framesJustRead;
                    }

                    if (!audioBuffer->playing)
                    {
                        framesRead = frameCount;
                        break;
                    }

                    // If we weren't able to read all the frames we requested, break
                    if (framesJustRead < framesToReadRightNow)
                    {
                        if (!audioBuffer->looping)
                        {
                            StopAudioBuffer(audioBuffer);
                            break;
                        }
                        else
                        {
                            // Should never get here, but just for safety,
                            // move the cursor position back to the start and continue the loop
                            audioBuffer->frameCursorPos = 0;
                            continue;
                        }
                    }
                }

                // If for some reason we weren't able to read every frame we'll need to break from the loop
                // Not doing this could theoretically put us into an infinite loop
                if (framesToRead > 0) break;
            }
        }
    }

    riqAudioProcessor* processor = AUDIO.mixedProcessor;
    while (processor)
    {
        processor->process(pFramesOut, frameCount);
        processor = processor->next;
    }

    ma_mutex_unlock(&AUDIO.System.lock);
}

// Get pointer to extension for a filename string (includes the dot: .png)
static const char* GetFileExtension(const char* fileName)
{
	const char* dot = strrchr(fileName, '.');

	if (!dot || dot == fileName) return NULL;

	return dot;
}

// Load data from file into a buffer
static unsigned char* LoadFileData(const char* fileName, unsigned int* bytesRead)
{
    unsigned char* data = NULL;
    *bytesRead = 0;

    if (fileName != NULL)
    {
        FILE* file = fopen(fileName, "rb");

        if (file != NULL)
        {
            // WARNING: On binary streams SEEK_END could not be found,
            // using fseek() and ftell() could not work in some (rare) cases
            fseek(file, 0, SEEK_END);
            int size = ftell(file);
            fseek(file, 0, SEEK_SET);

            if (size > 0)
            {
                data = (unsigned char*)RIQ_MALLOC(size * sizeof(unsigned char));

                // NOTE: fread() returns number of read elements instead of bytes, so we read [1 byte, size elements]
                unsigned int count = (unsigned int)fread(data, sizeof(unsigned char), size, file);
                *bytesRead = count;

                if (count != size) DEBUG_WARNING_FMT(unityLogPtr, "FILEIO: [%s] File partially loaded", fileName);
                else DEBUG_LOG_FMT(unityLogPtr, "FILEIO: [%s] File loaded successfully", fileName);
            }
            else DEBUG_WARNING_FMT(unityLogPtr, "FILEIO: [%s] Failed to read file", fileName);

            fclose(file);
        }
        else DEBUG_WARNING_FMT(unityLogPtr, "FILEIO: [%s] Failed to open file", fileName);
    }
    else DEBUG_WARNING(unityLogPtr, "FILEIO: File name provided is not valid");

    return data;
}

#pragma endregion

static void OnLog(void* pUserData, ma_uint32 level, const char* pMessage)
{
    // NOTE: All log messages from miniaudio are errors.
    DEBUG_ERROR_FMT(unityLogPtr, "miniaudio: %s", pMessage);
}