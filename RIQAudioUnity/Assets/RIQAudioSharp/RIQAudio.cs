using System;
using System.Runtime.InteropServices;

namespace RIQAudioUnity
{
    public static unsafe class RIQAudio
    {
        [DllImport("RIQAudio")]
        public static extern void RiqInitAudioDevice();
        [DllImport("RIQAudio")]
        public static extern void RiqCloseAudioDevice();
        [DllImport("RIQAudio")]
        public static extern bool IsRiqReady();


        [DllImport("RIQAudio")]
        private static extern Sound RiqLoadSound(sbyte* filePath);
        /// <summary>Load sound from file</summary>
        public static Sound RiqLoadSound(string filePath)
        {
            using var str1 = filePath.ToAnsiBuffer();
            return RiqLoadSound(str1.AsPointer());
        }

        [DllImport("RIQAudio")]
        public static extern Sound RiqLoadSoundFromWave(Wave wave);
        [DllImport("RIQAudio")]
        public static extern void RiqUnloadSound(Sound sound);
        [DllImport("RIQAudio")]
        public static extern void RiqPlaySound(Sound sound);

        [DllImport("RIQAudio")]
        private static extern Wave RiqLoadWave(sbyte* filePath);
        /// <summary>Load wave data from file</summary>
        public static Wave RiqLoadWave(string filePath)
        {
            using var str1 = filePath.ToAnsiBuffer();
            return RiqLoadWave(str1.AsPointer());
        }

        [DllImport("RIQAudio")]
        private static extern Wave RiqLoadWaveFromMemory(sbyte* filePath, byte* fileData, int dataSize);
        /// <summary>Load wave from managed memory, fileType refers to extension: i.e. ".wav"</summary>
        public static Wave RiqLoadWaveFromMemory(string fileType, byte[] fileData)
        {
            using var fileTypeNative = fileType.ToAnsiBuffer();

            fixed (byte* fileDataNative = fileData)
            {
                Wave wave = RiqLoadWaveFromMemory(fileTypeNative.AsPointer(), fileDataNative, fileData.Length);
                return wave;
            }
        }
    }

    /// <summary>
    /// Audio wave data
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct Wave
    {
        /// <summary>
        /// Total number of frames
        /// </summary>
        public uint FrameCount;

        /// <summary>
        /// Frequency (samples per second)
        /// </summary>
        public uint SampleRate;

        /// <summary>
        /// Bit depth (bits per sample): 8, 16, 32 (24 not supported)
        /// </summary>
        public uint SampleSize;

        /// <summary>
        /// Number of channels (1-mono, 2-stereo, ...)
        /// </summary>
        public int Channels;

        /// <summary>
        /// Buffer data pointer
        /// </summary>
        public void* Data;
    }

    /// <summary>
    /// Sound effects
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Sound
    {
        public AudioStream Stream;

        /// <summary>
        /// Total number of frames
        /// </summary>
        public uint FrameCount;
    }

    /// <summary>
    /// Useful to create custom audio streams not bound to a specific file
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct AudioStream
    {
        /// <summary>
        /// Pointer to (riqAudioBuffer) used by the audio system
        /// </summary>
        public IntPtr Buffer;

        /// <summary>
        /// Pointer to internal data processor, useful for audio effects
        /// </summary>
        public IntPtr Processor;

        /// <summary>
        /// Frequency (samples per second)
        /// </summary>
        public uint SampleRate;

        /// <summary>
        /// Bit depth (bits per sample): 8, 16, 32 (24 not supported)
        /// </summary>
        public uint SampleSize;

        /// <summary>
        /// Number of channels (1-mono, 2-stereo, ...)
        /// </summary>
        public uint Channels;
    }
}
